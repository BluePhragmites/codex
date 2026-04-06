#define _GNU_SOURCE

#include "mini_gnb_c/ue/ue_tun.h"

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <linux/if_tun.h>
#include <net/if.h>

static int mini_gnb_c_write_text_file(const char* path, const char* text) {
  int fd = -1;
  size_t remaining = 0u;
  const char* cursor = text;

  if (path == NULL || text == NULL) {
    return -1;
  }

  fd = open(path, O_WRONLY | O_CLOEXEC);
  if (fd < 0) {
    return -1;
  }

  remaining = strlen(text);
  while (remaining > 0u) {
    const ssize_t written = write(fd, cursor, remaining);
    if (written < 0) {
      const int saved_errno = errno;
      close(fd);
      errno = saved_errno;
      return -1;
    }
    cursor += (size_t)written;
    remaining -= (size_t)written;
  }

  if (close(fd) != 0) {
    return -1;
  }
  return 0;
}

static int mini_gnb_c_run_ip_command(char* const argv[]) {
  pid_t child_pid = -1;
  int status = 0;

  if (argv == NULL || argv[0] == NULL) {
    return -1;
  }

  child_pid = fork();
  if (child_pid < 0) {
    return -1;
  }
  if (child_pid == 0) {
    execvp(argv[0], argv);
    _exit(127);
  }

  if (waitpid(child_pid, &status, 0) != child_pid) {
    return -1;
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

static int mini_gnb_c_ue_tun_set_loopback_up(void) {
  char* const command[] = {"ip", "link", "set", "lo", "up", NULL};

  return mini_gnb_c_run_ip_command(command);
}

static int mini_gnb_c_ue_tun_enter_isolated_netns(void) {
  char map_text[64];

  if (geteuid() == 0) {
    if (unshare(CLONE_NEWNET) != 0) {
      return -1;
    }
    return mini_gnb_c_ue_tun_set_loopback_up();
  }

  if (unshare(CLONE_NEWUSER) != 0) {
    return -1;
  }
  if (mini_gnb_c_write_text_file("/proc/self/setgroups", "deny\n") != 0 && errno != ENOENT) {
    return -1;
  }
  if (snprintf(map_text, sizeof(map_text), "0 %u 1\n", (unsigned)getuid()) >= (int)sizeof(map_text)) {
    return -1;
  }
  if (mini_gnb_c_write_text_file("/proc/self/uid_map", map_text) != 0) {
    return -1;
  }
  if (snprintf(map_text, sizeof(map_text), "0 %u 1\n", (unsigned)getgid()) >= (int)sizeof(map_text)) {
    return -1;
  }
  if (mini_gnb_c_write_text_file("/proc/self/gid_map", map_text) != 0) {
    return -1;
  }
  if (setresgid(0, 0, 0) != 0) {
    return -1;
  }
  if (setresuid(0, 0, 0) != 0) {
    return -1;
  }
  if (unshare(CLONE_NEWNET) != 0) {
    return -1;
  }
  return mini_gnb_c_ue_tun_set_loopback_up();
}

static int mini_gnb_c_ue_tun_set_mtu_and_up(const char* ifname, const uint16_t mtu) {
  char mtu_text[16];
  char* const mtu_command[] = {"ip", "link", "set", "dev", (char*)ifname, "mtu", mtu_text, NULL};
  char* const up_command[] = {"ip", "link", "set", "dev", (char*)ifname, "up", NULL};

  if (ifname == NULL || ifname[0] == '\0') {
    return -1;
  }
  if (snprintf(mtu_text, sizeof(mtu_text), "%u", mtu) >= (int)sizeof(mtu_text)) {
    return -1;
  }
  if (mini_gnb_c_run_ip_command(mtu_command) != 0) {
    return -1;
  }
  return mini_gnb_c_run_ip_command(up_command);
}

static int mini_gnb_c_ue_tun_replace_ipv4_address(const char* ifname,
                                                  const uint8_t ue_ipv4[4],
                                                  const uint8_t prefix_len) {
  char cidr_text[32];
  char* const command[] = {"ip", "addr", "replace", cidr_text, "dev", (char*)ifname, NULL};

  if (ifname == NULL || ue_ipv4 == NULL || ifname[0] == '\0') {
    return -1;
  }
  if (snprintf(cidr_text,
               sizeof(cidr_text),
               "%u.%u.%u.%u/%u",
               ue_ipv4[0],
               ue_ipv4[1],
               ue_ipv4[2],
               ue_ipv4[3],
               prefix_len) >= (int)sizeof(cidr_text)) {
    return -1;
  }
  return mini_gnb_c_run_ip_command(command);
}

void mini_gnb_c_ue_tun_init(mini_gnb_c_ue_tun_t* tun) {
  if (tun == NULL) {
    return;
  }

  memset(tun, 0, sizeof(*tun));
  tun->fd = -1;
}

int mini_gnb_c_ue_tun_open(mini_gnb_c_ue_tun_t* tun, const mini_gnb_c_sim_config_t* sim) {
  struct ifreq ifr;

  if (tun == NULL || sim == NULL || !sim->ue_tun_enabled) {
    return -1;
  }
  mini_gnb_c_ue_tun_init(tun);
  tun->isolate_netns = sim->ue_tun_isolate_netns;
  tun->mtu = sim->ue_tun_mtu;
  tun->prefix_len = sim->ue_tun_prefix_len;

  if (tun->isolate_netns && mini_gnb_c_ue_tun_enter_isolated_netns() != 0) {
    return -1;
  }

  tun->fd = open("/dev/net/tun", O_RDWR | O_NONBLOCK | O_CLOEXEC);
  if (tun->fd < 0) {
    return -1;
  }

  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
  if (sim->ue_tun_name[0] != '\0') {
    const size_t ifname_length = strlen(sim->ue_tun_name);

    if (ifname_length >= sizeof(ifr.ifr_name)) {
      mini_gnb_c_ue_tun_close(tun);
      return -1;
    }
    memcpy(ifr.ifr_name, sim->ue_tun_name, ifname_length);
    ifr.ifr_name[ifname_length] = '\0';
  }
  if (ioctl(tun->fd, TUNSETIFF, &ifr) != 0) {
    mini_gnb_c_ue_tun_close(tun);
    return -1;
  }

  (void)snprintf(tun->ifname, sizeof(tun->ifname), "%s", ifr.ifr_name);
  tun->opened = true;
  if (mini_gnb_c_ue_tun_set_mtu_and_up(tun->ifname, tun->mtu) != 0) {
    mini_gnb_c_ue_tun_close(tun);
    return -1;
  }
  return 0;
}

int mini_gnb_c_ue_tun_configure_ipv4(mini_gnb_c_ue_tun_t* tun, const uint8_t ue_ipv4[4]) {
  if (tun == NULL || !tun->opened || ue_ipv4 == NULL) {
    return -1;
  }
  if (tun->configured && memcmp(tun->local_ipv4, ue_ipv4, sizeof(tun->local_ipv4)) == 0) {
    return 0;
  }
  if (mini_gnb_c_ue_tun_replace_ipv4_address(tun->ifname, ue_ipv4, tun->prefix_len) != 0) {
    return -1;
  }
  memcpy(tun->local_ipv4, ue_ipv4, sizeof(tun->local_ipv4));
  tun->configured = true;
  return 0;
}

int mini_gnb_c_ue_tun_read_packet(mini_gnb_c_ue_tun_t* tun,
                                  uint8_t* out_packet,
                                  const size_t out_size,
                                  size_t* out_length) {
  const ssize_t read_length = (tun != NULL && tun->opened && out_packet != NULL && out_size > 0u)
                                  ? read(tun->fd, out_packet, out_size)
                                  : -1;

  if (out_length != NULL) {
    *out_length = 0u;
  }
  if (tun == NULL || !tun->opened || out_packet == NULL || out_size == 0u) {
    return -1;
  }
  if (read_length < 0) {
    return (errno == EAGAIN || errno == EWOULDBLOCK) ? 1 : -1;
  }
  if (out_length != NULL) {
    *out_length = (size_t)read_length;
  }
  return 0;
}

int mini_gnb_c_ue_tun_write_packet(mini_gnb_c_ue_tun_t* tun, const uint8_t* packet, const size_t packet_length) {
  const ssize_t written = (tun != NULL && tun->opened && packet != NULL && packet_length > 0u)
                              ? write(tun->fd, packet, packet_length)
                              : -1;

  if (tun == NULL || !tun->opened || packet == NULL || packet_length == 0u) {
    return -1;
  }
  return written == (ssize_t)packet_length ? 0 : -1;
}

void mini_gnb_c_ue_tun_close(mini_gnb_c_ue_tun_t* tun) {
  if (tun == NULL) {
    return;
  }

  if (tun->fd >= 0) {
    close(tun->fd);
  }
  tun->fd = -1;
  tun->opened = false;
  tun->configured = false;
}
