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

#ifndef API_RBTREE_H_INCLUDED
#define API_RBTREE_H_INCLUDED

#define API_RB_BLACK    0
#define API_RB_RED      1

typedef struct api_rbnode_t {
    struct api_rbnode_t* parent;
    struct api_rbnode_t* left;
    struct api_rbnode_t* right;
    int color;
} api_rbnode_t;

typedef int(*api_rbnode_compare)(api_rbnode_t* node1, api_rbnode_t* node2);

api_rbnode_t* api_rbtree_next(api_rbnode_t* node);
api_rbnode_t* api_rbtree_first(api_rbnode_t* root);

void api_rbtree_insert(
    api_rbnode_t** root,
    api_rbnode_t* node,
    api_rbnode_compare compare);

void api_rbtree_remove(
    api_rbnode_t** root,
    api_rbnode_t* node,
    api_rbnode_compare compare);

api_rbnode_t* api_rbtree_search(
    api_rbnode_t* root,
    api_rbnode_t* value,
    api_rbnode_compare compare);

#endif // API_RBTREE_H_INCLUDED