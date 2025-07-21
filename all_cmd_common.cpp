#include "all_cmd_common.h"

string ConvertToString(char *token)
{
	string s(token);

	return s;
}

bool lookup_nickname_map(string nick_name)
{
	bool found = false;

	pthread_rwlock_rdlock(&map_rw_lock);
	if (g_nickname_map.find(nick_name) != g_nickname_map.end()) {
		found = true;
	}
	pthread_rwlock_unlock(&map_rw_lock);

	if (found) return true;
	else return false;
}

void insert_nickname_map(string nick_name, int wsfd)
{
	pthread_rwlock_wrlock(&map_rw_lock);

	g_nickname_map[nick_name] = wsfd;

	pthread_rwlock_unlock(&map_rw_lock);
}
