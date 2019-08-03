#include "bargain_v0/bargain.hpp"

#if 0
#include <cstdio>

struct bargain_v0::STM_private::Local {
  static void print_tree(access_base_t *root, int level = 2) {
    if (root)
      print_tree(root->children[0], level + 2);
    for (int i = 0; i < level; ++i)
      fprintf(stderr, " ");
    if (root)
      fprintf(stderr, "%p\n", static_cast<void *>(root->atom));
    else
      fprintf(stderr, "null\n");
    if (root)
      print_tree(root->children[1], level + 2);
  }
};
#endif

std::atomic<uint64_t> bargain_v0::STM_private::s_clock(0);

bargain_v0::STM_private::access_base_t *
bargain_v0::STM_private::insert_base(access_base_t *root,
                                     access_base_t *access) {
  if (!root) {
    access->children[0] = nullptr;
    access->children[1] = nullptr;
    return access;
  }

  access_base_t *side_root[2];
  access_base_t **side_near[2] = {&side_root[0], &side_root[1]};

  auto access_atom = access->atom;

  while (true) {
    auto root_atom = root->atom;

    if (access_atom == root_atom) {
      if (!access->write)
        access = root;
      *side_near[0] = root->children[0];
      *side_near[1] = root->children[1];
      access->children[0] = side_root[0];
      access->children[1] = side_root[1];
      return access;
    }

    if (access_atom < root_atom) {
      constexpr int o = 1, n = 0;

      auto next = root->children[n];

      if (!next) {
        *side_near[n] = nullptr;
        *side_near[o] = root->children[o];
        root->children[o] = side_root[o];
        access->children[n] = side_root[n];
        access->children[o] = root;
        return access;
      }

      auto next_atom = next->atom;

      if (access_atom < next_atom) {
        root->children[n] = next->children[o];
        next->children[o] = root;
        root = next;
        next = root->children[n];

        if (!next) {
          *side_near[o] = root;
          root->children[n] = nullptr;
          *side_near[n] = nullptr;
          access->children[0] = side_root[0];
          access->children[1] = side_root[1];
          return access;
        }
      }

      *side_near[o] = root;
      side_near[o] = &root->children[n];
      root = next;
    } else {
      constexpr int o = 0, n = 1;

      auto next = root->children[n];

      if (!next) {
        *side_near[n] = nullptr;
        *side_near[o] = root->children[o];
        root->children[o] = side_root[o];
        access->children[n] = side_root[n];
        access->children[o] = root;
        return access;
      }

      auto next_atom = next->atom;

      if (access_atom > next_atom) {
        root->children[n] = next->children[o];
        next->children[o] = root;
        root = next;
        next = root->children[n];

        if (!next) {
          *side_near[o] = root;
          root->children[n] = nullptr;
          *side_near[n] = nullptr;
          access->children[0] = side_root[0];
          access->children[1] = side_root[1];
          return access;
        }
      }

      *side_near[o] = root;
      side_near[o] = &root->children[n];
      root = next;
    }
  }
}

bool bargain_v0::STM_private::try_commit(uint64_t t, access_base_t *root) {
  if (!root)
    return true;

  access_base_t *writes;
  access_base_t *reads;

  {
    access_base_t **writes_tail = &writes;
    access_base_t **reads_tail = &reads;

    access_base_t *prev = nullptr;
    do {
      auto left = root->children[0];
      if (left) {
        root->children[0] = prev;
        prev = root;
        root = left;
      } else {
        if (root->write) {
          auto atom = root->atom;
          auto s = atom->lock.load();
          if (t < s || !atom->lock.compare_exchange_strong(
                           s, ~s, std::memory_order_acquire))
            break;
          *writes_tail = root;
          writes_tail = &root->children[1];
        } else {
          *reads_tail = root;
          reads_tail = &root->children[1];
        }

        root = root->children[1];
        if (!root) {
          root = prev;
          if (prev) {
            prev = prev->children[0];
            root->children[0] = nullptr;
          }
        }
      }
    } while (root);

    *writes_tail = nullptr;
    *reads_tail = nullptr;
  }

  if (!root) {
    auto u = ++s_clock;

    if (u != t + 1) {
      for (auto it = reads; it; it = it->children[1]) {
        if (t < it->atom->lock.load()) {
          root = it;
          break;
        }
      }
    }

    if (!root)
      for (auto it = writes; it; it = it->children[1])
        it->write(u, it);
  }

  if (root) {
    for (auto it = writes; it; it = it->children[1]) {
      auto atom = it->atom;
      atom->lock.store(~atom->lock.load(std::memory_order_relaxed),
                       std::memory_order_relaxed);
    }
  }
  return !root;
}
