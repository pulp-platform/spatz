// Copyright 2025 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: Diyou Shen <dishen@iis.ee.ethz.ch>

#include "llist.h"


node_t* createNode (int id, const double data) {
  node_t* newnode       = (node_t* ) snrt_l3alloc(sizeof(node_t));
  newnode->payload.id   = id;
  newnode->payload.data = data;
  newnode->next         = NULL;

  return newnode;
}


int insertNode (node_t** head, node_t* newNode) {
  if (newNode == NULL) {
        // Cannot insert a NULL node.
        return 1;
    }

    // If the list is empty, the new node becomes the head
    if (*head == NULL) {
        *head = newNode;
        return 0;
    }

    // Traverse to the end of the list and append the new node
    node_t* current = *head;
    while (current->next != NULL) {
        current = current->next;
    }
    current->next = newNode;
    newNode->next = NULL; // Ensure the new node is the last node

    return 0;
}

int deleteNode (node_t** head, int id) {
  if (*head == NULL) {
    // empty linked list
    return 1;
  }

  node_t* tempnode = *head;
  node_t* prevnode = NULL;

  if (tempnode->payload.id == id) {
    *head = tempnode->next;
    // In theory we should free this node, but currently we don't have this function support
    return 0;
  }


  while (tempnode != NULL && tempnode->payload.id != id) {
    prevnode = tempnode;
    tempnode = tempnode->next;
  }

  if (tempnode == NULL) {
    // ID is not found in the linked-list
    return 2;
  }

  // Remove the node
  prevnode->next = tempnode->next;
  // In theory we should free this node, but currently we don't have this function support
  return 0;
}

void sortList(node_t** head) {
  if (*head == NULL || (*head)->next == NULL) {
    // List is empty or has only one node, no sorting needed
    return;
  }

  // Bubble sort logic for simplicity (can be optimized if needed)
  int swapped;
  node_t *ptr1, *lptr = NULL;

  do {
    swapped = 0;
    ptr1 = *head;

    while (ptr1->next != lptr) {
      if (ptr1->payload.id > ptr1->next->payload.id) {
        // Swap the payloads
        payload_t temp = ptr1->payload;
        ptr1->payload = ptr1->next->payload;
        ptr1->next->payload = temp;
        swapped = 1;
      }
      ptr1 = ptr1->next;
    }
    lptr = ptr1; // Move the lptr marker to the last sorted node
  } while (swapped);
}

int traverseList (node_t* head) {
  if (head == NULL) {
    // List is empty
    return 1;
  }

  volatile node_t* tempnode = head;
  while (tempnode != NULL) {
    tempnode = tempnode->next;
  }
  return 0;
}

int traverseList_vec (node_t* head, uint32_t num_nodes, uint32_t num_lists) {
  if (num_lists == 0 || num_nodes == 0) {
    // No linked lists
    return 1;
  }

  int base_addr = 0;
  uint32_t avl   = num_lists;
  uint32_t vl;

  for (; avl > 0; avl -= vl) {
    // Stripmine
    asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
    // preload the head address in the first iteration
    asm volatile("vle32.v v8,  (%0)" ::"r"(head));

    for (uint32_t step = 0; step < num_nodes; step = step + 2) {
      // Indexed Load the next nodes of the linked-list
      // The base address is zero because the head contains the full address
      asm volatile("vluxei32.v v16, (%0), v8" ::"r"(base_addr));

      asm volatile("vluxei32.v v8, (%0), v16" ::"r"(base_addr));
    }

    head += vl;
  }

  return 0;
}

