/* Copyright (c) 2014, Artak Khnkoyan <artak.khnkoyan@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "api_rbtree.h"

#define RB_COLOR(n) (n ? n->color : API_RB_BLACK)

api_rbnode_t* api_rbnode_grangparent(api_rbnode_t* node)
{
    if (node->parent)
        return node->parent->parent;

    return 0;
}

api_rbnode_t* api_rbnode_uncle(api_rbnode_t* node)
{
    api_rbnode_t* grandparent = api_rbnode_grangparent(node);

    if (!grandparent)
        return 0;

    if (node->parent == grandparent->left)
        return grandparent->right;

    return grandparent->left;
}

api_rbnode_t* api_rbnode_sibling(api_rbnode_t* node)
{
    if (node == node->parent->left)
        return node->parent->right;

    return node->parent->left;
}

void api_rbtree_rotate_left(api_rbnode_t** root, api_rbnode_t* node)
{
    api_rbnode_t* right = node->right;

    node->right = right->left;
    if (node->right)
        right->left->parent = node;

    right->parent = node->parent;
    if (right->parent)
    {
        if (node == right->parent->left)
            node->parent->left = right;
        else
            node->parent->right = right;
    }
    else
    {
        *root = right;
    }

    right->left = node;
    node->parent = right;
}

void api_rbtree_rotate_right(api_rbnode_t** root, api_rbnode_t* node)
{
    api_rbnode_t* left = node->left;

    node->left = left->right;
    if (node->left)
        left->right->parent = node;

    left->parent = node->parent;
    if (left->parent)
    {
        if (node == left->parent->left)
            left->parent->left = left;
        else
            left->parent->right = left;
    }
    else
    {
        *root = left;
    }

    left->right = node;
    node->parent = left;
}

void api_rbtree_insert_bal(api_rbnode_t** root, api_rbnode_t* node)
{
    api_rbnode_t* grandparent;
    api_rbnode_t* uncle;

    if (*root == node)
    {
        node->color = API_RB_BLACK;
    }
    else if (API_RB_RED == RB_COLOR(node->parent))
    {
        uncle = api_rbnode_uncle(node);
        grandparent = api_rbnode_grangparent(node);

        if (uncle && API_RB_RED == uncle->color)
        {
            node->parent->color = API_RB_BLACK;
            uncle->color = API_RB_BLACK;
            grandparent->color = API_RB_RED;

            api_rbtree_insert_bal(root, grandparent);
            return;
        }

        if (node == node->parent->right && node->parent == grandparent->left)
        {
            api_rbtree_rotate_left(root, node->parent);
            node = node->left;
        }
        else if (node == node->parent->left && node->parent == grandparent->right)
        {
            api_rbtree_rotate_right(root, node->parent);
            node = node->right;
        }

        grandparent = api_rbnode_grangparent(node);
        node->parent->color = API_RB_BLACK;
        grandparent->color = API_RB_RED;

        if (node == node->parent->left && node->parent == grandparent->left)
            api_rbtree_rotate_right(root, grandparent);
        else
            api_rbtree_rotate_left(root, grandparent);
    }
}

void api_rbtree_remove_bal(api_rbnode_t** root, api_rbnode_t* node)
{
    api_rbnode_t* sibling;

    if (!node->parent)
        return;

    sibling = api_rbnode_sibling(node);

    if (API_RB_RED == RB_COLOR(sibling))
    {
        node->parent->color = API_RB_RED;
        sibling->color = API_RB_BLACK;

        if (node == node->parent->left)
            api_rbtree_rotate_left(root, node->parent);
        else
            api_rbtree_rotate_right(root, node->parent);

        sibling = api_rbnode_sibling(node);
    }

    if (API_RB_BLACK == RB_COLOR(node->parent) &&
        API_RB_BLACK == RB_COLOR(sibling) &&
        API_RB_BLACK == RB_COLOR(sibling->left) &&
        API_RB_BLACK == RB_COLOR(sibling->right))
    {
        sibling->color = API_RB_RED;
        api_rbtree_remove_bal(root, node->parent);
        return;
    }

    if (API_RB_RED == RB_COLOR(node->parent) &&
        API_RB_BLACK == RB_COLOR(sibling) &&
        API_RB_BLACK == RB_COLOR(sibling->left) &&
        API_RB_BLACK == RB_COLOR(sibling->right))
    {
        sibling->color = API_RB_RED;
        node->parent->color = API_RB_BLACK;
    }
    else
    { 
        if (node->parent->left == node &&
            API_RB_BLACK == RB_COLOR(sibling->right) &&
            API_RB_RED == RB_COLOR(sibling->left))
        {
            sibling->color = API_RB_RED;
            sibling->left->color = API_RB_BLACK;
            api_rbtree_rotate_right(root, sibling);
        }
        else if (node->parent->right == node &&
            API_RB_BLACK == RB_COLOR(sibling->left) &&
            API_RB_RED == RB_COLOR(sibling->right))
        {
            sibling->color = API_RB_BLACK;
            api_rbtree_rotate_left(root, sibling);
        }

        sibling = api_rbnode_sibling(node);
        sibling->color = RB_COLOR(node->parent);
        node->parent->color = API_RB_BLACK;

        if (node->parent->left == node)
        {
            sibling->right->color = API_RB_BLACK;
            api_rbtree_rotate_left(root, node->parent);
        }
        else
        {
            sibling->left->color = API_RB_BLACK;
            api_rbtree_rotate_right(root, node->parent);
        }
    }
}

void api_rbtree_swap(api_rbnode_t* node1, api_rbnode_t* node2)
{
    api_rbnode_t* temp = node1->parent;
    int color;

    // swap parents

    node1->parent = node2->parent;
    node2->parent = temp;

    if (node1->parent)
    {
        if (node1->parent->left == node2)
            node1->parent->left = node1;
        else
            node1->parent->right = node1;
    }

    if (node2->parent)
    {
        if (node2->parent->left == node1)
            node2->parent->left = node2;
        else
            node2->parent->right = node2;
    }

    // swap lefts

    temp = node1->left;
    node1->left = node2->left;
    if (node1->left)
        node1->left->parent = node1;

    node2->left = temp;
    if (node2->left)
        node2->left->parent = node2;

    // swap rights

    temp = node1->right;
    node1->right = node2->right;
    if (node1->right)
        node1->right->parent = node1;

    node2->right = temp;
    if (node2->right)
        node2->right->parent = node2;

    // swap color

    color = node1->color;
    node1->color = node2->color;
    node2->color = color;
}

api_rbnode_t* api_rbtree_next(api_rbnode_t* node)
{
    api_rbnode_t* next = 0;

    if (node->right)
    {
        next = node->right;
    }
    else if (node->parent)
    {
        if (node != node->parent->right)
            next = node->parent->right;
    }

    if (next)
    {
        while (next->left)
            next = next->left;
    }

    return next;
}

api_rbnode_t* api_rbtree_first(api_rbnode_t* root)
{
    api_rbnode_t* first = root;

    if (first)
    {
        while (first->left)
            first = first->left;
    }

    return first;
}

void api_rbtree_insert(
    api_rbnode_t** root,
    api_rbnode_t* node,
    api_rbnode_compare compare)
{
    api_rbnode_t* current;
    api_rbnode_t* parent;

    current = *root;

    while (current)
    {
        parent = current;

        if (compare(node, current) < 0)
            current = current->left;
        else
            current = current->right;
    }

    node->parent = 0;
    node->left = 0;
    node->right = 0;
    node->color = API_RB_RED;

    if (!*root)
    {
        *root = node;
    }
    else
    {
        node->parent = parent;

        if (compare(node, parent) < 0)
            parent->left = node;
        else
            parent->right = node;
    }

    api_rbtree_insert_bal(root, node);
}

void api_rbtree_remove(
        api_rbnode_t** root,
        api_rbnode_t* node,
        api_rbnode_compare compare)
{
    api_rbnode_t* child;
    api_rbnode_t* prev;
    api_rbnode_t phantom;

    phantom.left = 0;
    phantom.right = 0;
    phantom.parent = 0;
    phantom.color = API_RB_BLACK;

    if (node->left && node->right)
    {
        prev = node->left;

        while (prev->right)
            prev = prev->right;

        api_rbtree_swap(node, prev);

        if (*root == prev)
            *root = node;
        else if (*root == node)
            *root = prev;
    }

    if (node->left) 
        child = node->left;
    else if (node->right)
        child = node->right;
    else
        child = &phantom;

    if (!node->parent)
        *root = child;
    else
    {
        if (node == node->parent->left)
            node->parent->left = child;
        else
            node->parent->right = child;
    }

    if (child != node->left)
        child->left = node->left;

    if (child != node->right)
        child->right = node->right;

    child->parent = node->parent;
    node->left = 0;
    node->right = 0;
    node->parent = 0;

    if (API_RB_BLACK == RB_COLOR(node))
    {
        if (API_RB_RED == RB_COLOR(child))
            child->color = API_RB_BLACK;
        else
            api_rbtree_remove_bal(root, child);
    }

    if (&phantom == child)
    {
        if (child->parent)
        {
            if (child == child->parent->left)
                child->parent->left = 0;
            else
                child->parent->right = 0;
        }
        else if (*root == &phantom) 
            *root = 0;
    }
}

api_rbnode_t* api_rbtree_search(
    api_rbnode_t* root,
    api_rbnode_t* value,
    api_rbnode_compare compare)
{
    api_rbnode_t* node = root;
    int comparision;

    while (node)
    {
        comparision = compare(value, node);

        if (comparision == 0)
            return node;

        if (comparision < 0)
            node = node->left;
        else
            node = node->right;
    }

    return 0;
}