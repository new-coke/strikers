#ifndef MSL_TREE_H_
#define MSL_TREE_H_

#include "PowerPC_EABI_Support/MSL_C++/MSL_Common/utility.h"
#include "PowerPC_EABI_Support/MSL_C++/MSL_Common/msl_memory.h"
#include "stdio.h"
#include "stdlib.h"
#include "types.h"

namespace std
{

template <class Arg1, class Arg2, class Result>
struct binary_function
{
};

// PORT: MSVC's <type_traits> already defines std::less.
#if !defined(_MSC_VER)
template <class T>
struct less : public binary_function<T, T, bool>
{
};
#endif

template <int N>
class __red_black_tree
{
public:
    struct node_base;
    struct anchor
    {
        void* left_;
        anchor()
            : left_(0)
        {
        }
    };
    struct node_base : public anchor
    {
        void* right_;
        void* parent_;

        node_base* parent() const { return (node_base*)((uintptr_t)parent_ & ~1); }
        node_base* grandparent() const { return parent()->parent(); }
        bool is_red() const { return ((uintptr_t)parent_ & 1) == 1; }
        void set_red() { parent_ = (void*)((uintptr_t)parent_ | 1); }
        void set_black() { parent_ = (void*)((uintptr_t)parent_ & ~1); }
    };

    static void rotate_right(node_base* x, node_base*& root);
    static void rotate_left(node_base* x, node_base*& root);
    static void balance_insert(node_base* x, node_base* root);
};

template <int N>
void __red_black_tree<N>::balance_insert(node_base* x, node_base* root)
{
    node_base* y;
    x->set_red();
    while (x != root && x->parent()->is_red())
    {
        node_base*& left = (node_base*&)x->grandparent()->left_;
        if (x->parent() == left)
        {
            y = (node_base*)x->grandparent()->right_;
            if (y != 0 && y->is_red())
            {
                x->parent()->set_black();
                y->set_black();
                x = x->grandparent();
                x->set_red();
            }
            else
            {
                if (x == (node_base*)x->parent()->right_)
                {
                    x = x->parent();
                    rotate_left(x, root);
                }
                x->parent()->set_black();
                x->grandparent()->set_red();
                rotate_right(x->grandparent(), root);
            }
        }
        else
        {
            y = left;
            if (y != 0 && y->is_red())
            {
                x->parent()->set_black();
                y->set_black();
                x = x->grandparent();
                x->set_red();
            }
            else
            {
                if (x == (node_base*)x->parent()->left_)
                {
                    x = x->parent();
                    rotate_right(x, root);
                }
                x->parent()->set_black();
                x->grandparent()->set_red();
                rotate_left(x->grandparent(), root);
            }
        }
    }
    root->set_black();
}

template <int N>
void __red_black_tree<N>::rotate_left(node_base* x, node_base*& root)
{
    node_base* y = (node_base*)x->right_;
    if (root == x)
    {
        root = y;
    }
    x->right_ = y->left_;
    node_base* yl = (node_base*)y->left_;
    if (yl != 0)
    {
        yl->parent_ = (void*)((uintptr_t)x | ((uintptr_t)yl->parent_ & 1));
    }
    y->parent_ = (void*)(((uintptr_t)x->parent_ & ~1) | ((uintptr_t)y->parent_ & 1));
    node_base* parent = (node_base*)((uintptr_t)x->parent_ & ~1);
    if (x == (node_base*)parent->left_)
    {
        parent->left_ = y;
    }
    else
    {
        parent->right_ = y;
    }
    y->left_ = x;
    x->parent_ = (void*)((uintptr_t)y | ((uintptr_t)x->parent_ & 1));
}

template <int N>
void __red_black_tree<N>::rotate_right(node_base* x, node_base*& root)
{
    node_base* y = (node_base*)x->left_;
    if (root == x)
    {
        root = y;
    }
    x->left_ = y->right_;
    node_base* yr = (node_base*)y->right_;
    if (yr != 0)
    {
        yr->parent_ = (void*)((uintptr_t)x | ((uintptr_t)yr->parent_ & 1));
    }
    y->parent_ = (void*)(((uintptr_t)x->parent_ & ~1) | ((uintptr_t)y->parent_ & 1));
    node_base* parent = (node_base*)((uintptr_t)x->parent_ & ~1);
    if (x == (node_base*)parent->left_)
    {
        parent->left_ = y;
    }
    else
    {
        parent->right_ = y;
    }
    y->right_ = x;
    x->parent_ = (void*)((uintptr_t)y | ((uintptr_t)x->parent_ & 1));
}

template <class T, class Compare, class Allocator>
class __tree : private __red_black_tree<1>
{
public:
    struct node : public __red_black_tree<1>::node_base
    {
        T data_;
    };

    class iterator
    {
    public:
        node* ptr_;
        iterator(node* p)
            : ptr_(p)
        {
        }
    };

    __tree(const Compare& comp, const Allocator& alloc);
    ~__tree();
    Allocator& alloc();
    std::allocator<node>& node_alloc();
    void clear();
    void destroy(node* n);

    template <class Key, class Value>
    T& find_or_insert(const Key& key);

    template <class Key>
    iterator find(const Key& x);

    node* insert_node_at(node* p, bool leftchild, bool is_leftmost, const T& x);

private:
    Metrowerks::details::compressed_pair_imp<Allocator, unsigned long, 1> alloc_;
    Metrowerks::details::compressed_pair_imp<std::allocator<node>, __red_black_tree<1>::anchor, 1> node_alloc_;
    Metrowerks::details::compressed_pair_imp<Compare, node*, 0> comp_;
};

template <class T, class Compare, class Allocator>
__tree<T, Compare, Allocator>::__tree(const Compare& comp, const Allocator& alloc)
    : alloc_()
    , node_alloc_()
    , comp_(comp, (node*)&node_alloc_.second())
{
}

template <class T, class Compare, class Allocator>
inline
__tree<T, Compare, Allocator>::~__tree()
{
    node* n = (node*)node_alloc_.second().left_;
    if (n != 0)
        destroy(n);
}

template <class T, class Compare, class Allocator>
inline
Allocator& __tree<T, Compare, Allocator>::alloc()
{
    return alloc_.first();
}

template <class T, class Compare, class Allocator>
inline
std::allocator<typename __tree<T, Compare, Allocator>::node>&
__tree<T, Compare, Allocator>::node_alloc()
{
    return node_alloc_.first();
}

template <class T, class Compare, class Allocator>
void __tree<T, Compare, Allocator>::clear()
{
    node* n = (node*)node_alloc_.second().left_;
    if (n != 0)
    {
        destroy(n);
        alloc_.second() = 0;
        node_alloc_.second().left_ = 0;
        comp_.second() = (node*)&node_alloc_.second();
    }
}

template <class T, class Compare, class Allocator>
void __tree<T, Compare, Allocator>::destroy(node* __p)
{
    if (__p->left_ != 0)
        destroy(static_cast<node*>(__p->left_));
    if (__p->right_ != 0)
        destroy(static_cast<node*>(__p->right_));
    alloc().destroy(&__p->data_);
    node_alloc().deallocate(__p, 1);
}

template <class T, class Compare, class Allocator>
template <class Key>
typename __tree<T, Compare, Allocator>::iterator
__tree<T, Compare, Allocator>::find(const Key& x)
{
    node* i = (node*)node_alloc_.second().left_;
    node* j = (node*)&node_alloc_.second();
    while (i != 0)
    {
        if (!(i->data_.first < x))
        {
            j = i;
            i = (node*)i->left_;
        }
        else
        {
            i = (node*)i->right_;
        }
    }
    if (j == (node*)&node_alloc_.second() || x < j->data_.first)
    {
        return iterator((node*)&node_alloc_.second());
    }
    return iterator(j);
}

template <class T, class Compare, class Allocator>
template <class Key, class Value>
T& __tree<T, Compare, Allocator>::find_or_insert(const Key& key)
{
    node* prev = 0;
    node* p = (node*)&node_alloc_.second();
    node* n = (node*)node_alloc_.second().left_;
    bool leftchild = true;
    bool is_leftmost = true;

    while (n != 0)
    {
        p = n;
        if (key < n->data_.first)
        {
            n = (node*)n->left_;
            leftchild = true;
        }
        else
        {
            prev = n;
            n = (node*)n->right_;
            leftchild = false;
            is_leftmost = false;
        }
    }

    if (prev == 0 || prev->data_.first < key)
    {
        T x(key, Value());
        return insert_node_at(p, leftchild, is_leftmost, x)->data_;
    }

    return prev->data_;
}

template <class T, class Compare, class Allocator>
typename __tree<T, Compare, Allocator>::node*
__tree<T, Compare, Allocator>::insert_node_at(node* p, bool leftchild, bool is_leftmost, const T& x)
{
    if (alloc_.second() > 0xfffffffeU)
    {
        fprintf(stderr, "tree::insert length error\n");
        abort();
    }

    node* new_node = (node*)::operator new(sizeof(node));
    if (new_node == 0)
    {
        fprintf(stderr, "Memory allocation failure");
        abort();
    }

    void* data = &new_node->data_;
    new (data) T(x);
    new_node->right_ = 0;
    new_node->left_ = 0;
    new_node->parent_ = (void*)((uintptr_t)p | ((uintptr_t)new_node->parent_ & 1));

    if (leftchild)
        p->left_ = new_node;
    else
        p->right_ = new_node;

    ++alloc_.second();
    balance_insert(new_node, (node_base*)node_alloc_.second().left_);

    if (is_leftmost)
        comp_.second() = new_node;

    return new_node;
}

template <class Key, class Value, class Compare = less<Key>, class Allocator = allocator<pair<const Key, Value> > >
class map
{
public:
    class value_compare : public binary_function<pair<const Key, Value>, pair<const Key, Value>, bool>
    {
    protected:
        Compare comp;
    };

    typedef typename __tree<pair<const Key, Value>, value_compare, Allocator>::iterator iterator;

    map()
        : tree_(value_compare(), Allocator())
    {
    }

    iterator find(const Key& x)
    {
        return tree_.find(x);
    }

    __tree<pair<const Key, Value>, value_compare, Allocator> tree_;
};

} // namespace std

#endif
