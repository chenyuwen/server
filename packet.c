#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "c_hashmap/hashmap.h"
#include "crc32/crc32.h"
#include "jansson/jansson.h"
#include "server_configs.h"
#include "packet.h"
#include "server.h"
#include "mlog.h"
#include "server_errno.h"

int build_simplify_json(json_t *json, const char *method, int err)
{
	json_object_del(json, "method");
	json_object_del(json, "errno");

	json_object_set_new(json, "method", json_string(method));
	json_object_set_new(json, "errno", json_integer(err));
	return 0;
}

int respond_method_not_found(struct server *sv, struct client *ct, json_t *json)
{
	json_t *rsp_json = json_object();
	struct raw_packet *packet = alloc_raw_packet(sv, ct);

	build_simplify_json(rsp_json, "null", -SERR_NOT_METHOD);
	json_to_raw_packet(rsp_json, PACKET_TYPE_UNENCRY, packet);
	respond_raw_packet(sv, ct, packet);

	/*free json*/
	json_delete(rsp_json);
	free_raw_packet(sv, ct, packet);
	return 0;
}

int json_to_raw_packet(json_t *json, int type, struct raw_packet *packet)
{
	struct crc32_raw_packet *crc32_packet = (struct crc32_raw_packet *)packet;

	packet->head.type = type;
	packet->head.packet_len = json_dumpb(json, packet->buffer, RAW_PACKET_BUFFER_MAX, 0);
	packet->head.crc32 = crc32_classic(&crc32_packet->crcdata, sizeof_raw_packet(packet) -
		sizeof(struct crc32_raw_packet));
	return 0;
}

int call_method(struct server *sv, struct client *ct, json_t *json, const char *method)
{
	int i = 0, ret = 0;
	int (*handler)(struct server *, struct client *, json_t *);

	if(sv->dump) printf("Enter:%s\n", method);
	hashmap_get(sv->methods_map, (char *)method, (any_t *)&handler);
	if(handler == NULL) {
		respond_method_not_found(sv, ct, json);
		ret = -1;
		goto out;
	}
	ret = handler(sv, ct, json);
	if(sv->dump) printf("Exit:%s\n", method);

out:
	return ret;
}

int dispatch_packet(struct server *sv, struct client *ct, struct raw_packet *packet)
{
	json_t *json, *method_json;
	const char *tmp = NULL;
	json_error_t json_err;
	int ret = 0;

	json = json_loadb(packet->buffer, packet->head.packet_len, 0, &json_err);
	if(json == NULL) {
		mlog("json_loadb failed.\n");
		return -1;
	}

	method_json = json_object_get(json, "method");
	tmp = json_string_value(method_json);
	if(tmp == NULL) {
		mlog("the methods is null.\n");
		return -1;
	}
	mlog("From %s:%s\n", ct->ipaddr, packet->buffer);
	ret = call_method(sv, ct, json, tmp);
	json_delete(json);
	return ret;
}

int sizeof_raw_packet(struct raw_packet *packet)
{
	return sizeof(struct raw_packet_head) + packet->head.packet_len;
}

struct raw_packet *alloc_raw_packet(struct server *sv,
	struct client *ct)
{
	if(sv->dump) memset(ct->respond, 0, sizeof(ct->respond));
	return (struct raw_packet *)ct->respond;
}

int free_raw_packet(struct server *sv, struct client *ct,
	struct raw_packet *packet)
{
	return 0;
}

int respond_raw_packet(struct server *sv, struct client *ct,
	struct raw_packet *packet)
{
	mlog("To %s:%s\n", ct->ipaddr, packet->buffer);
	return write(ct->fd, (void *)packet, sizeof_raw_packet(packet));
}

