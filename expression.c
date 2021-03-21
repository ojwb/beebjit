#include "expression.h"

#include "log.h"
#include "util.h"
#include "util_container.h"

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct expression_struct {
  struct util_tree_struct* p_tree;
  struct util_tree_node_struct* p_current_node;
};

enum {
  k_expression_node_integer = 1,
  k_expression_node_var = 2,
  k_expression_node_equal = 3,
  k_expression_node_plus = 4,
  k_expression_node_minus = 5,
};

struct expression_struct*
expression_create(void) {
  struct expression_struct* p_expression =
      util_mallocz(sizeof(struct expression_struct));
  p_expression->p_tree = util_tree_alloc();
  return p_expression;
}

void
expression_destroy(struct expression_struct* p_expression) {
  util_tree_free(p_expression->p_tree);
  util_free(p_expression);
}

static void
expression_clear(struct expression_struct* p_expression) {
  util_tree_free(p_expression->p_tree);
  p_expression->p_tree = util_tree_alloc();
  p_expression->p_current_node = NULL;
}

static int32_t
expression_get_precedence(int32_t type) {
  int32_t ret = 0;
  switch (type) {
  case k_expression_node_integer:
  case k_expression_node_var:
    ret = 3;
    break;
  case k_expression_node_plus:
  case k_expression_node_minus:
    ret = 2;
    break;
  case k_expression_node_equal:
    ret = 1;
    break;
  default:
    assert(0);
    break;
  }
  return ret;
}

static void
expression_process_token(struct expression_struct* p_expression,
                         const char* p_token_str) {
  int32_t new_precedence;
  struct util_tree_struct* p_tree = p_expression->p_tree;
  struct util_tree_node_struct* p_new_node = NULL;
  struct util_tree_node_struct* p_parent_node = p_expression->p_current_node;
  char c = p_token_str[0];
  int32_t type = 0;
  assert(!isspace(c));
  if (isdigit(c)) {
    int64_t val = strtoll(p_token_str, NULL, 10);
    type = k_expression_node_integer;
    p_new_node = util_tree_node_alloc(k_expression_node_integer);
    util_tree_node_set_int_value(p_new_node, val);
  } else if (isalpha(c)) {
    /* TODO. */
  } else {
    if (!strcmp(p_token_str, "==")) {
      type = k_expression_node_equal;
    } else if (!strcmp(p_token_str, "+")) {
      type = k_expression_node_plus;
    } else if (!strcmp(p_token_str, "-")) {
      type = k_expression_node_minus;
    }
    if (type == 0) {
      log_do_log(k_log_misc, k_log_warning, "unknown operator %s", p_token_str);
    }
    p_new_node = util_tree_node_alloc(type);
  }

  new_precedence = expression_get_precedence(type);
  while (1) {
    int32_t curr_type;
    int32_t curr_precedence;
    if (p_parent_node == NULL) {
      break;
    }
    curr_type = util_tree_node_get_type(p_parent_node);
    curr_precedence = expression_get_precedence(curr_type);
    if (new_precedence > curr_precedence) {
      break;
    }
    p_parent_node = util_tree_node_get_parent(p_parent_node);
  }
  if (p_parent_node == NULL) {
    struct util_tree_node_struct* p_old_root = util_tree_get_root(p_tree);
    util_tree_set_root(p_tree, p_new_node);
    if (p_old_root != NULL) {
      util_tree_node_add_child(p_new_node, p_old_root);
    }
  } else {
    util_tree_node_add_child(p_parent_node, p_new_node);
  }

  p_expression->p_current_node = p_new_node;
}

int64_t
expression_parse(struct expression_struct* p_expression,
                 const char* p_expr_str) {
  uint32_t i;
  uint32_t len;
  size_t full_len = strlen(p_expr_str);

  (void) p_expression;
  (void) p_expr_str;

  if (full_len >= INT_MAX) {
    util_bail("!");
  }
  len = full_len;

  expression_clear(p_expression);

  i = 0;
  while (i < len) {
    char token_buf[256];
    uint32_t token_len;
    char c;
    uint32_t max_token_len = (sizeof(token_buf) - 1);
    while (isspace(p_expr_str[i])) {
      i++;
    }
    token_len = 0;
    while (isalpha(c = p_expr_str[i]) && (token_len < max_token_len)) {
      token_buf[token_len++] = c;
      i++;
    }
    if (token_len > 0) {
      token_buf[token_len] = '\0';
      expression_process_token(p_expression, &token_buf[0]);
      token_len = 0;
    }
    while (isdigit(c = p_expr_str[i]) && (token_len < max_token_len)) {
      token_buf[token_len++] = c;
      i++;
    }
    if (token_len > 0) {
      token_buf[token_len] = '\0';
      expression_process_token(p_expression, &token_buf[0]);
      token_len = 0;
    }
    while ((c = p_expr_str[i]) != '\0' &&
           !isspace(c) &&
           !isalpha(c) &&
           !isdigit(c) &&
           (token_len < max_token_len)) {
      token_buf[token_len++] = c;
      i++;
    }
    if (token_len > 0) {
      token_buf[token_len] = '\0';
      expression_process_token(p_expression, &token_buf[0]);
      token_len = 0;
    }
  }

  return 0;
}

uint32_t
expression_get_tree_size(struct expression_struct* p_expression) {
  return util_tree_get_tree_size(p_expression->p_tree);
}

static int64_t
expression_execute_node(struct util_tree_node_struct* p_node) {
  int64_t ret = 0;
  int32_t type = util_tree_node_get_type(p_node);
  uint32_t num_children = util_tree_node_get_num_children(p_node);
  struct util_tree_node_struct* p_child_node_1 = NULL;
  struct util_tree_node_struct* p_child_node_2 = NULL;

  if (num_children == 2) {
    p_child_node_1 = util_tree_node_get_child(p_node, 0);
    p_child_node_2 = util_tree_node_get_child(p_node, 1);
  }

  switch (type) {
  case k_expression_node_integer:
    ret = util_tree_node_get_int_value(p_node);
    break;
  case k_expression_node_plus:
    if (num_children == 2) {
      ret = expression_execute_node(p_child_node_1);
      ret += expression_execute_node(p_child_node_2);
    }
    break;
  case k_expression_node_minus:
    if (num_children == 2) {
      ret = expression_execute_node(p_child_node_1);
      ret -= expression_execute_node(p_child_node_2);
    }
    break;
  default:
    assert(0);
  }

  return ret;
}

int64_t
expression_execute(struct expression_struct* p_expression) {
  struct util_tree_node_struct* p_node;
  if (p_expression->p_tree == NULL) {
    return 0;
  }
  p_node = util_tree_get_root(p_expression->p_tree);
  if (p_node == NULL) {
    return 0;
  }
  return expression_execute_node(p_node);
}

#include "test-expression.c"