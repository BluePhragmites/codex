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
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <linux/if_tun.h>
#include <net/if.h>

static int mini_gnb_c_write_text_file_existing(const char* path, const char* text) {
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

static int mini_gnb_c_write_text_file_create(const char* path, const char* text) {
  int fd = -1;
  size_t remaining = 0u;
  const char* cursor = text;

  if (path == NULL || text == NULL) {
    return -1;
  }

  fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
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

static int mini_gnb_c_ue_tun_make_mounts_private(void) {
  return mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL);
}

static bool mini_gnb_c_ue_tun_is_permission_error(const int error_code) {
  return error_code == EACCES || error_code == EPERM || error_code == EROFS;
}

static int mini_gnb_c_ue_tun_enter_isolated_netns(mini_gnb_c_ue_tun_t* tun) {
  char map_text[64];
  const uid_t outer_uid = getuid();
  const gid_t outer_gid = getgid();

  if (tun == NULL) {
    return -1;
  }
  if (geteuid() == 0) {
    if (unshare(CLONE_NEWNET) != 0) {
      return -1;
    }
    return mini_gnb_c_ue_tun_set_loopback_up();
  }

  if (unshare(CLONE_NEWUSER) != 0) {
    return -1;
  }
  if (mini_gnb_c_write_text_file_existing("/proc/self/setgroups", "deny\n") != 0 && errno != ENOENT) {
    return -1;
  }
  if (snprintf(map_text, sizeof(map_text), "0 %u 1\n", (unsigned)outer_uid) >= (int)sizeof(map_text)) {
    return -1;
  }
  if (mini_gnb_c_write_text_file_existing("/proc/self/uid_map", map_text) != 0) {
    return -1;
  }
  if (snprintf(map_text, sizeof(map_text), "0 %u 1\n", (unsigned)outer_gid) >= (int)sizeof(map_text)) {
    return -1;
  }
  if (mini_gnb_c_write_text_file_existing("/proc/self/gid_map", map_text) != 0) {
    return -1;
  }
  if (setresgid(0, 0, 0) != 0) {
    return -1;
  }
  if (setresuid(0, 0, 0) != 0) {
    return -1;
  }
  if (unshare(CLONE_NEWNET | CLONE_NEWNS) != 0) {
    return -1;
  }
  if (mini_gnb_c_ue_tun_make_mounts_private() != 0) {
    return -1;
  }
  tun->mount_ns_isolated = true;
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

static int mini_gnb_c_ensure_dir(const char* path) {
  if (path == NULL || path[0] == '\0') {
    return -1;
  }
  if (mkdir(path, 0755) == 0 || errno == EEXIST) {
    return 0;
  }
  return -1;
}

static int mini_gnb_c_ue_tun_replace_default_route(const char* ifname) {
  char* const command[] = {"ip", "route", "replace", "default", "dev", (char*)ifname, NULL};

  if (ifname == NULL || ifname[0] == '\0') {
    return -1;
  }
  return mini_gnb_c_run_ip_command(command);
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

static int mini_gnb_c_ue_tun_publish_named_netns(mini_gnb_c_ue_tun_t* tun) {
  char netns_path[MINI_GNB_C_MAX_PATH];
  int fd = -1;
  int saved_errno = 0;

  if (tun == NULL || tun->netns_name[0] == '\0') {
    return 0;
  }
  if (mini_gnb_c_ensure_dir("/var/run/netns") != 0) {
    return mini_gnb_c_ue_tun_is_permission_error(errno) ? 1 : -1;
  }
  if (snprintf(netns_path, sizeof(netns_path), "/var/run/netns/%s", tun->netns_name) >= (int)sizeof(netns_path)) {
    return -1;
  }

  fd = open(netns_path, O_RDONLY | O_CREAT | O_CLOEXEC, 0644);
  if (fd < 0) {
    return mini_gnb_c_ue_tun_is_permission_error(errno) ? 1 : -1;
  }
  if (close(fd) != 0) {
    return -1;
  }
  if (mount("/proc/self/ns/net", netns_path, NULL, MS_BIND, NULL) != 0) {
    saved_errno = errno;
    (void)unlink(netns_path);
    return mini_gnb_c_ue_tun_is_permission_error(saved_errno) ? 1 : -1;
  }
  tun->netns_published = true;
  return 0;
}

static int mini_gnb_c_ue_tun_bind_local_resolv_conf(mini_gnb_c_ue_tun_t* tun) {
  char resolv_text[128];

  if (tun == NULL || tun->dns_server_ipv4[0] == '\0' || !tun->mount_ns_isolated) {
    return -1;
  }
  if (snprintf(tun->dns_bind_source_path,
               sizeof(tun->dns_bind_source_path),
               "/tmp/mini_gnb_c_resolv_%ld.conf",
               (long)getpid()) >= (int)sizeof(tun->dns_bind_source_path)) {
    return -1;
  }
  if (snprintf(resolv_text,
               sizeof(resolv_text),
               "nameserver %s\noptions timeout:1 attempts:1\n",
               tun->dns_server_ipv4) >= (int)sizeof(resolv_text)) {
    return -1;
  }
  if (mini_gnb_c_write_text_file_create(tun->dns_bind_source_path, resolv_text) != 0) {
    tun->dns_bind_source_path[0] = '\0';
    return -1;
  }
  if (mount(tun->dns_bind_source_path, "/etc/resolv.conf", NULL, MS_BIND, NULL) != 0) {
    (void)unlink(tun->dns_bind_source_path);
    tun->dns_bind_source_path[0] = '\0';
    return -1;
  }
  tun->dns_bind_mounted = true;
  tun->dns_configured = true;
  return 0;
}

static int mini_gnb_c_ue_tun_write_netns_resolv_conf(mini_gnb_c_ue_tun_t* tun) {
  char etc_netns_dir[MINI_GNB_C_MAX_PATH];
  char resolv_path[MINI_GNB_C_MAX_PATH];
  char resolv_text[128];

  if (tun == NULL || tun->netns_name[0] == '\0' || tun->dns_server_ipv4[0] == '\0') {
    return 0;
  }
  if (mini_gnb_c_ensure_dir("/etc/netns") != 0) {
    return -1;
  }
  if (snprintf(etc_netns_dir, sizeof(etc_netns_dir), "/etc/netns/%s", tun->netns_name) >= (int)sizeof(etc_netns_dir)) {
    return -1;
  }
  if (mini_gnb_c_ensure_dir(etc_netns_dir) != 0) {
    return -1;
  }
  if (snprintf(resolv_path, sizeof(resolv_path), "%s/resolv.conf", etc_netns_dir) >= (int)sizeof(resolv_path)) {
    return -1;
  }
  if (snprintf(resolv_text,
               sizeof(resolv_text),
               "nameserver %s\noptions timeout:1 attempts:1\n",
               tun->dns_server_ipv4) >= (int)sizeof(resolv_text)) {
    return -1;
  }
  if (mini_gnb_c_write_text_file_create(resolv_path, resolv_text) != 0) {
    return -1;
  }
  tun->dns_configured = true;
  return 0;
}

static int mini_gnb_c_ue_tun_configure_dns(mini_gnb_c_ue_tun_t* tun) {
  if (tun == NULL || tun->dns_server_ipv4[0] == '\0') {
    return 0;
  }
  if (tun->dns_configured) {
    return 0;
  }
  if (tun->netns_published && tun->netns_name[0] != '\0') {
    return mini_gnb_c_ue_tun_write_netns_resolv_conf(tun);
  }
  return mini_gnb_c_ue_tun_bind_local_resolv_conf(tun);
}

static void mini_gnb_c_ue_tun_cleanup_netns(const mini_gnb_c_ue_tun_t* tun) {
  char netns_path[MINI_GNB_C_MAX_PATH];

  if (tun == NULL || !tun->netns_published || tun->netns_name[0] == '\0') {
    return;
  }
  if (snprintf(netns_path, sizeof(netns_path), "/var/run/netns/%s", tun->netns_name) >= (int)sizeof(netns_path)) {
    return;
  }
  (void)umount(netns_path);
  (void)unlink(netns_path);
}

static void mini_gnb_c_ue_tun_cleanup_dns(const mini_gnb_c_ue_tun_t* tun) {
  char etc_netns_dir[MINI_GNB_C_MAX_PATH];
  char resolv_path[MINI_GNB_C_MAX_PATH];

  if (tun == NULL || !tun->dns_configured) {
    return;
  }
  if (tun->dns_bind_mounted) {
    (void)umount("/etc/resolv.conf");
    if (tun->dns_bind_source_path[0] != '\0') {
      (void)unlink(tun->dns_bind_source_path);
    }
    return;
  }
  if (tun->netns_name[0] == '\0') {
    return;
  }
  if (snprintf(etc_netns_dir, sizeof(etc_netns_dir), "/etc/netns/%s", tun->netns_name) >= (int)sizeof(etc_netns_dir)) {
    return;
  }
  if (snprintf(resolv_path, sizeof(resolv_path), "%s/resolv.conf", etc_netns_dir) >= (int)sizeof(resolv_path)) {
    return;
  }
  (void)unlink(resolv_path);
  (void)rmdir(etc_netns_dir);
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
  tun->default_route_enabled = sim->ue_tun_add_default_route;
  tun->mtu = sim->ue_tun_mtu;
  tun->prefix_len = sim->ue_tun_prefix_len;
  (void)snprintf(tun->netns_name, sizeof(tun->netns_name), "%s", sim->ue_tun_netns_name);
  (void)snprintf(tun->dns_server_ipv4, sizeof(tun->dns_server_ipv4), "%s", sim->ue_tun_dns_server_ipv4);

  if (tun->isolate_netns && mini_gnb_c_ue_tun_enter_isolated_netns(tun) != 0) {
    return -1;
  }
  if (tun->isolate_netns) {
    const int publish_result = mini_gnb_c_ue_tun_publish_named_netns(tun);

    if (publish_result < 0) {
      mini_gnb_c_ue_tun_close(tun);
      return -1;
    }
    if (publish_result > 0) {
      fprintf(stderr,
              "UE TUN could not publish named netns '%s' under /var/run/netns; continuing with anonymous isolated "
              "namespace.\n",
              tun->netns_name);
      fflush(stderr);
    }
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
  if (tun->default_route_enabled && mini_gnb_c_ue_tun_replace_default_route(tun->ifname) != 0) {
    return -1;
  }
  tun->default_route_configured = tun->default_route_enabled;
  if (mini_gnb_c_ue_tun_configure_dns(tun) != 0) {
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

const char* mini_gnb_c_ue_tun_netns_name(const mini_gnb_c_ue_tun_t* tun) {
  if (tun == NULL || !tun->netns_published || tun->netns_name[0] == '\0') {
    return NULL;
  }
  return tun->netns_name;
}

void mini_gnb_c_ue_tun_close(mini_gnb_c_ue_tun_t* tun) {
  if (tun == NULL) {
    return;
  }

  if (tun->fd >= 0) {
    close(tun->fd);
  }
  tun->fd = -1;
  mini_gnb_c_ue_tun_cleanup_netns(tun);
  mini_gnb_c_ue_tun_cleanup_dns(tun);
  tun->opened = false;
  tun->configured = false;
  tun->mount_ns_isolated = false;
  tun->netns_published = false;
  tun->dns_configured = false;
  tun->dns_bind_mounted = false;
  tun->dns_bind_source_path[0] = '\0';
  tun->default_route_configured = false;
}
