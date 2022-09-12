#include "common/gpio.h"

#include <fcntl.h>
#include <unistd.h>

#include <cstring>
#include <linux/gpio.h>
#include <sys/ioctl.h>

#include "common/util.h"
#include "common/swaglog.h"

// We assume that all pins have already been exported on boot,
// and that we have permission to write to them.

const std::string gpiochip_path = "/dev/gpiochip0";

int gpio_init(int pin_nr, bool output) {
  char pin_dir_path[50];
  int pin_dir_path_len = snprintf(pin_dir_path, sizeof(pin_dir_path),
                           "/sys/class/gpio/gpio%d/direction", pin_nr);
  if(pin_dir_path_len <= 0) {
    return -1;
  }
  const char *value = output ? "out" : "in";
  return util::write_file(pin_dir_path, (void*)value, strlen(value));
}

int gpio_set(int pin_nr, bool high) {
  char pin_val_path[50];
  int pin_val_path_len = snprintf(pin_val_path, sizeof(pin_val_path),
                           "/sys/class/gpio/gpio%d/value", pin_nr);
  if(pin_val_path_len <= 0) {
    return -1;
  }
  return util::write_file(pin_val_path, (void*)(high ? "1" : "0"), 1);
}

int gpiochip_get_ro_value_fd(int pin_nr, EdgeType etype) {
  int fd = open(gpiochip_path.c_str(), O_RDONLY);
  if (fd < 0) {
    LOGE("Error opening gpiochip fd")
    return -1;
  }

  // Setup event
  struct gpioevent_request rq;
  rq.lineoffset = pin_nr;
  rq.handleflags = GPIOHANDLE_REQUEST_INPUT;

  // rq.eventflags = (etype == EdgeType::Rising) ? GPIOEVENT_REQUEST_RISING_EDGE : GPIOEVENT_REQUEST_FALLING_EDGE;
  /* Requesting both edges as the data ready pulse from the lsm6ds sensor is
     very short(75us) and is mostly detected as falling edge instead of rising.
     So if it is detected as rising the following falling edge is skipped. */
  rq.eventflags = GPIOEVENT_REQUEST_BOTH_EDGES;

  strncpy(rq.consumer_label, "sensord", std::size(rq.consumer_label) - 1);
  int ret = ioctl(fd, GPIO_GET_LINEEVENT_IOCTL, &rq);
  if (ret == -1) {
      LOGE("Unable to get line event from ioctl : %s", strerror(errno));
      close(fd);
      return -1;
  }

  close(fd);
  return rq.fd;
}
