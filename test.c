//#define INNER(x) (x + 1)
//#define TEST(x) (x * 2)
//#define TEST2 / 69
//#define _concat(x, y) x + y
//#define concat(x, y) INNER(x) + y
//int a = concat(test,fest);
#define WOBJDATA_STRUCT_DEF(name) struct name \
	{ \
		float x, y; \
		float vel_x, vel_y; \
		float custom_floats[2]; \
		int custom_ints[2]; \
		float speed, jump; \
		float strength, health; \
		int money; \
		int anim_frame; \
		int type; \
		int item; \
		int flags; \
		int last_sound_played; \
		int last_sound_uuid; \
		unsigned char link_node; \
		int link_uuid; \
		CNM_BOX hitbox; \
		unsigned char node_id; \
		int uuid; \
	};


typedef struct CNM_BOX CNM_BOX;
typedef struct OBJGRID_OBJECT OBJGRID_OBJECT;
typedef struct _WOBJ WOBJ;
typedef struct SPAWNER SPAWNER;
typedef unsigned short uint16_t;

#define NETGAME_MAX_NODES 8
#define NETGAME_MAX_HISTORY 12

WOBJDATA_STRUCT_DEF(wobjdata)

struct _WOBJ
{
	WOBJDATA_STRUCT_DEF()
	struct
	{
		int owned;
		OBJGRID_OBJECT obj;
		struct _WOBJ *next, *last;
	} internal;

	void *local_data;
	void (*on_destroy)(WOBJ *);

	SPAWNER *parent_spawner;
	int dropped_death_item;
	int interpolate;
	int interp_frame;
	float interp_x, interp_y;
	float smooth_x, smooth_y;

	struct wobjdata history[NETGAME_MAX_HISTORY];
	int history_frames[NETGAME_MAX_HISTORY];
	uint16_t nodes_sent_to[NETGAME_MAX_NODES];
};

//int a = concat(test, pre);
