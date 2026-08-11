set(WOLFSSL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../third_party/wolfSSL")
set(WOLFSSL_SETTINGS "${WOLFSSL_ROOT}/IDE/Espressif/ESP-IDF")
file(TO_CMAKE_PATH "$ENV{IDF_PATH}" IDF_ROOT)
file(GLOB WOLFSSL_SRC
    "${WOLFSSL_ROOT}/src/*.c"
    "${WOLFSSL_ROOT}/wolfcrypt/src/*.c"
    "${WOLFSSL_ROOT}/wolfcrypt/src/port/Espressif/*.c")
set(WOLFSSL_EXCLUDE
    "${WOLFSSL_ROOT}/src/bio.c"
    "${WOLFSSL_ROOT}/src/conf.c"
    "${WOLFSSL_ROOT}/src/misc.c"
    "${WOLFSSL_ROOT}/src/pk.c"
    "${WOLFSSL_ROOT}/src/ssl_asn1.c"
    "${WOLFSSL_ROOT}/src/ssl_bn.c"
    "${WOLFSSL_ROOT}/src/ssl_certman.c"
    "${WOLFSSL_ROOT}/src/ssl_crypto.c"
    "${WOLFSSL_ROOT}/src/ssl_load.c"
    "${WOLFSSL_ROOT}/src/ssl_misc.c"
    "${WOLFSSL_ROOT}/src/ssl_p7p12.c"
    "${WOLFSSL_ROOT}/src/ssl_sess.c"
    "${WOLFSSL_ROOT}/src/ssl_sk.c"
    "${WOLFSSL_ROOT}/src/x509.c"
    "${WOLFSSL_ROOT}/src/x509_str.c"
    "${WOLFSSL_ROOT}/wolfcrypt/src/evp.c"
    "${WOLFSSL_ROOT}/wolfcrypt/src/misc.c")
list(REMOVE_ITEM WOLFSSL_SRC ${WOLFSSL_EXCLUDE})
idf_component_register(
    SRCS ${WOLFSSL_SRC}
    INCLUDE_DIRS "${WOLFSSL_ROOT}" "${WOLFSSL_ROOT}/wolfssl" "${WOLFSSL_SETTINGS}"
        "${IDF_ROOT}/components/freertos/FreeRTOS-Kernel/include/freertos"
    REQUIRES lwip)
target_compile_definitions(${COMPONENT_LIB} PUBLIC WOLFSSL_USER_SETTINGS WOLFSSL_ESPIDF)
