#include <string.h>

#include "test_helpers.h"

#include "mini_gnb_c/config/config_loader.h"
#include "mini_gnb_c/radio/radio_frontend.h"

void test_radio_frontend_initializes_mock_backend(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  mini_gnb_c_config_t config;
  mini_gnb_c_radio_frontend_t radio;

  mini_gnb_c_default_config_path(config_path, sizeof(config_path));
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected config to load");
  mini_gnb_c_require(mini_gnb_c_radio_frontend_init(&radio, &config.rf, &config.sim) == 0,
                     "expected mock radio frontend to initialize");
  mini_gnb_c_require(mini_gnb_c_radio_frontend_is_ready(&radio), "expected mock radio frontend ready");
  mini_gnb_c_require(mini_gnb_c_radio_frontend_kind(&radio) == MINI_GNB_C_RADIO_BACKEND_MOCK,
                     "expected mock backend kind");
  mini_gnb_c_require(strcmp(mini_gnb_c_radio_frontend_driver_name(&radio), "mock") == 0,
                     "expected mock backend name");
  mini_gnb_c_require(mini_gnb_c_radio_frontend_error(&radio)[0] == '\0', "expected no mock init error");
  mini_gnb_c_radio_frontend_shutdown(&radio);
}

void test_radio_frontend_rejects_unsupported_backend_name(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  mini_gnb_c_config_t config;
  mini_gnb_c_radio_frontend_t radio;

  mini_gnb_c_default_config_path(config_path, sizeof(config_path));
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected config to load");
  (void)snprintf(config.rf.device_driver, sizeof(config.rf.device_driver), "%s", "bogus");
  mini_gnb_c_require(mini_gnb_c_radio_frontend_init(&radio, &config.rf, &config.sim) != 0,
                     "expected unsupported backend to fail");
  mini_gnb_c_require(!mini_gnb_c_radio_frontend_is_ready(&radio), "expected unsupported backend not ready");
  mini_gnb_c_require(mini_gnb_c_radio_frontend_kind(&radio) == MINI_GNB_C_RADIO_BACKEND_UNKNOWN,
                     "expected unknown backend kind");
  mini_gnb_c_require(strstr(mini_gnb_c_radio_frontend_error(&radio), "unsupported") != NULL,
                     "expected unsupported-backend init error");
}
