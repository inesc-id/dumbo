#ifndef TEST_SPINS_H_GUARD
#define TEST_SPINS_H_GUARD

#define TEST_MSG_INIT() \
  char test_msg_var[1024]; \
  test_msg_var[0] = '\n'; \
//

#define TEST_MSG_WRITE(...) \
  sprintf(test_msg_var, __VA_ARGS__); \
//

#define TEST_MSG_WRITE_FORMAT(...) \
  sprintf(test_msg_var, __VA_ARGS__); \
//

#define TEST_MSG_APPEND(...) \
  sprintf(test_msg_var + strlen(test_msg_var), "%s", __VA_ARGS__); \
//

#define TEST_MSG_APPEND_FORMAT(...) \
  sprintf(test_msg_var + strlen(test_msg_var), __VA_ARGS__); \
//

#define TEST_MSG_GET() \
  test_msg_var \
//

#define TEST_MSG_EXIT() /* empty */

#endif /* TEST_SPINS_H_GUARD */
