#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <yajl/yajl_parse.h>

typedef enum {
  EVENT_INVALID = 0,
  EVENT_MAP,
  EVENT_ARRAY,
  KEY_NETWORKS,
  KEY_TYPE,
  KEY_IP_ADDRESS,
  KEY_ROUTES,
  KEY_NETMASK,
  KEY_NETWORK,
  KEY_GATEWAY,
  KEY_SERVICES,
  KEY_ADDRESS,
  KEY__UNDEF,
} event_t;

typedef enum {
  SERVICE_INVALID = 0,
  SERVICE_DNS,
} service_t;

#define MAX_STACK_EVENTS 5

typedef event_t path_t[MAX_STACK_EVENTS];

typedef struct {
  path_t path;
  int path_index;

  int af;
  struct {
    char addr[INET6_ADDRSTRLEN + 1];
    char mask[INET6_ADDRSTRLEN + 1];
  } address;
  struct {
    char addr[INET6_ADDRSTRLEN + 1];
    char mask[INET6_ADDRSTRLEN + 1];
    char next_hop[INET6_ADDRSTRLEN + 1];
  } route;
  struct {
    service_t type;
    char addr[INET6_ADDRSTRLEN + 1];
  } service;
} ctx_t;

static const struct {
  const path_t networks;
  const path_t networks__type;
  const path_t networks__ip_address;
  const path_t networks__netmask;
  const path_t networks__routes;
  const path_t networks__routes__network;
  const path_t networks__routes__netmask;
  const path_t networks__routes__gateway;
  const path_t services;
  const path_t services__type;
  const path_t services__address;
} path = {
  { KEY_NETWORKS, EVENT_ARRAY },
  { KEY_NETWORKS, EVENT_ARRAY, KEY_TYPE },
  { KEY_NETWORKS, EVENT_ARRAY, KEY_IP_ADDRESS },
  { KEY_NETWORKS, EVENT_ARRAY, KEY_NETMASK },
  { KEY_NETWORKS, EVENT_ARRAY, KEY_ROUTES, EVENT_ARRAY },
  { KEY_NETWORKS, EVENT_ARRAY, KEY_ROUTES, EVENT_ARRAY, KEY_NETWORK },
  { KEY_NETWORKS, EVENT_ARRAY, KEY_ROUTES, EVENT_ARRAY, KEY_NETMASK },
  { KEY_NETWORKS, EVENT_ARRAY, KEY_ROUTES, EVENT_ARRAY, KEY_GATEWAY },
  { KEY_SERVICES, EVENT_ARRAY },
  { KEY_SERVICES, EVENT_ARRAY, KEY_TYPE },
  { KEY_SERVICES, EVENT_ARRAY, KEY_ADDRESS },
};

#define stack_push(_event_type)                           \
  do {                                                    \
    if (ctx->path_index == MAX_STACK_EVENTS) {            \
      fprintf(stderr, "maximum json depth exceeded\n");   \
      return 0;                                           \
    }                                                     \
    ctx->path[ctx->path_index++] = _event_type;           \
  } while (0)

#define stack_pop()                                       \
  do {                                                    \
    assert(ctx->path_index != 0);                         \
    ctx->path[--ctx->path_index] = EVENT_INVALID;         \
  } while (0)

static inline int
path_cmp(const event_t *p1, const event_t *p2)
{
  for (int i = 0; i < MAX_STACK_EVENTS; i++)
    if (p1[i] != p2[i])
      return 1;
  return 0;
}

static inline int
parse_type_af(const uint8_t *buf, size_t len)
{
  if (len == 4 && memcmp(buf, "ipv4", len) == 0)
    return AF_INET;
  else if (len == 4 && memcmp(buf, "ipv6", len) == 0)
    return AF_INET6;
  else
    return AF_UNSPEC;
}

#define copy_addr_str(_dst)                                 \
  do {                                                      \
    if (len > INET6_ADDRSTRLEN) {                           \
      fprintf(stderr, "addr str exceeds maximum length\n"); \
      return 0;                                             \
    }                                                       \
    memcpy(_dst, buf, len);                                 \
    _dst[len] = 0;                                          \
  } while (0)

static inline int
parse_type_service(const uint8_t *buf, size_t len)
{
  if (len == 3 && memcmp(buf, "dns", len) == 0)
    return SERVICE_DNS;
  else
    return SERVICE_INVALID;
}

static int
event_string(void *ptr, const uint8_t *buf, size_t len)
{
  ctx_t *ctx = ptr;

  if (path_cmp(ctx->path, path.networks__type) == 0)
    ctx->af = parse_type_af(buf, len);
  else if (path_cmp(ctx->path, path.networks__ip_address) == 0)
    copy_addr_str(ctx->address.addr);
  else if (path_cmp(ctx->path, path.networks__netmask) == 0)
    copy_addr_str(ctx->address.mask);
  else if (path_cmp(ctx->path, path.networks__routes__network) == 0)
    copy_addr_str(ctx->route.addr);
  else if (path_cmp(ctx->path, path.networks__routes__netmask) == 0)
    copy_addr_str(ctx->route.mask);
  else if (path_cmp(ctx->path, path.networks__routes__gateway) == 0)
    copy_addr_str(ctx->route.next_hop);
  else if (path_cmp(ctx->path, path.services__type) == 0)
    ctx->service.type = parse_type_service(buf, len);
  else if (path_cmp(ctx->path, path.services__address) == 0)
    copy_addr_str(ctx->service.addr);

  return 1;
}

static int
event_map_key(void *ptr, const uint8_t *buf, size_t len)
{
  ctx_t *ctx = ptr;

  if (len == 8 && memcmp("networks", buf, len) == 0)
    ctx->path[ctx->path_index - 1] = KEY_NETWORKS;
  else if (len == 4 && memcmp("type", buf, len) == 0)
    ctx->path[ctx->path_index - 1] = KEY_TYPE;
  else if (len == 10 && memcmp("ip_address", buf, len) == 0)
    ctx->path[ctx->path_index - 1] = KEY_IP_ADDRESS;
  else if (len == 6 && memcmp("routes", buf, len) == 0)
    ctx->path[ctx->path_index - 1] = KEY_ROUTES;
  else if (len == 7 && memcmp("netmask", buf, len) == 0)
    ctx->path[ctx->path_index - 1] = KEY_NETMASK;
  else if (len == 7 && memcmp("network", buf, len) == 0)
    ctx->path[ctx->path_index - 1] = KEY_NETWORK;
  else if (len == 7 && memcmp("gateway", buf, len) == 0)
    ctx->path[ctx->path_index - 1] = KEY_GATEWAY;
  else if (len == 8 && memcmp("services", buf, len) == 0)
    ctx->path[ctx->path_index - 1] = KEY_SERVICES;
  else if (len == 7 && memcmp("address", buf, len) == 0)
    ctx->path[ctx->path_index - 1] = KEY_ADDRESS;
  else
    ctx->path[ctx->path_index - 1] = KEY__UNDEF;

  return 1;
}

static int
event_start_map(void *ptr)
{
  ctx_t *ctx = ptr;
  stack_push(EVENT_MAP);
  return 1;
}

static inline int
prefix_length(int af, void *buf)
{
  switch (af) {
  case AF_INET:
    return __builtin_popcountl(*(uint32_t *)buf);
  case AF_INET6:
    return __builtin_popcountll(*(uint64_t *)buf) + __builtin_popcountll(*(((uint64_t *)buf) + 1));
  default:
    return -1;
  }
}

static int
print_address(ctx_t *ctx)
{
  if (ctx->af != AF_INET && ctx->af != AF_INET6)
    return -1;

  if (*ctx->address.addr == 0 || *ctx->address.mask == 0)
    return -1;

  uint8_t buf[(sizeof (struct in6_addr))] = {0};

  int ret = inet_pton(ctx->af, ctx->address.mask, buf);
  if (ret == 0)
    return -1;

  int prefix_len = prefix_length(ctx->af, buf);
  printf("address %s %d\n", ctx->address.addr, prefix_len);

  *ctx->address.addr = 0;
  *ctx->address.mask = 0;
  ctx->af = AF_UNSPEC;

  return 0;
}

static int
print_route(ctx_t *ctx)
{
  if (ctx->af != AF_INET && ctx->af != AF_INET6)
    return -1;

  if (*ctx->route.addr == 0 || *ctx->route.mask == 0 || *ctx->route.next_hop == 0)
    return -1;

  uint8_t buf[(sizeof (struct in6_addr))] = {0};

  int ret = inet_pton(ctx->af, ctx->route.mask, buf);
  if (ret == 0)
    return -1;

  int prefix_len = prefix_length(ctx->af, buf);
  printf("route %s %d %s\n", ctx->route.addr, prefix_len, ctx->route.next_hop);

  *ctx->route.addr = 0;
  *ctx->route.mask = 0;
  *ctx->route.next_hop = 0;

  return 0;
}

static void
print_service(ctx_t *ctx)
{
  switch (ctx->service.type) {
  case SERVICE_DNS:
    printf("resolver %s\n", ctx->service.addr);
    break;
  default:
    break;
  }

  ctx->service.type == SERVICE_INVALID;
  *ctx->service.addr = 0;
}

static int
event_end_map(void *ptr)
{
  ctx_t *ctx = ptr;

  stack_pop();

  if (path_cmp(ctx->path, path.networks__routes) == 0) {
    if (print_route(ctx) < 0) {
      fprintf(stderr, "invalid route: json schema violation\n");
      return 0;
    }
  } else if (path_cmp(ctx->path, path.networks) == 0) {
    if (print_address(ctx) < 0) {
      fprintf(stderr, "invalid address: json schema violation\n");
      return 0;
    }
  } else if (path_cmp(ctx->path, path.services) == 0)
    print_service(ctx);

  return 1;
}

static int
event_start_array(void *ptr)
{
  ctx_t *ctx = ptr;
  stack_push(EVENT_ARRAY);
  return 1;
}

static int
event_end_array(void *ptr)
{
  ctx_t *ctx = ptr;
  stack_pop();
  return 1;
}

static yajl_callbacks callbacks = {
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  event_string,
  event_start_map,
  event_map_key,
  event_end_map,
  event_start_array,
  event_end_array
};

#define READ_BUF_SIZE (16 * 1024)

int
main(void)
{
  yajl_handle handle;
  yajl_status status;
  uint8_t buf[READ_BUF_SIZE];
  ssize_t buf_len;
  ctx_t ctx = {0};
  int retval = 0;

  handle = yajl_alloc(&callbacks, NULL, &ctx);

  while (1) {
    buf_len = read(0, buf, (sizeof (buf)));
    if (buf_len == 0)
      break;
    else if (buf_len < 0) {
      perror("read");
      retval = 1;
      break;
    } else {
      status = yajl_parse(handle, buf, buf_len);
      if (status != yajl_status_ok) {
        retval = 1;
        break;
      }
    }
  }

  status = yajl_complete_parse(handle);
  if (status != yajl_status_ok) {
    retval = 1;
    uint8_t *str = yajl_get_error(handle, 1, buf, buf_len);
    fprintf(stderr, "%s", str);
    yajl_free_error(handle, str);
  }

  yajl_free(handle);

  return retval;
}
