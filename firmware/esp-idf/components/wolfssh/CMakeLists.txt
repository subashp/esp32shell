set(WOLFSSH_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../third_party/wolfSSH")
set(WOLFSSL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../third_party/wolfSSL")
file(GLOB WOLFSSH_SRC "${WOLFSSH_ROOT}/src/*.c")
list(REMOVE_ITEM WOLFSSH_SRC "${WOLFSSH_ROOT}/src/misc.c")
idf_component_register(
    SRCS ${WOLFSSH_SRC}
    INCLUDE_DIRS "${WOLFSSH_ROOT}" "${WOLFSSH_ROOT}/wolfssh" "${WOLFSSL_ROOT}" "${WOLFSSL_ROOT}/wolfssl"
    REQUIRES wolfssl lwip)
target_compile_definitions(${COMPONENT_LIB} PUBLIC WOLFSSL_USER_SETTINGS WOLFSSH_USER_SETTINGS)
