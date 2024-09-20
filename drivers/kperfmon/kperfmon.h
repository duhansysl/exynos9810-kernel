#define FLAG_NOTHING			0
#define FLAG_READING			1
#define USE_WORKQUEUE			1
#define NOT_USED			0

#define byte				unsigned char

struct tRingBuffer {
	byte *data;
	long length;
	long start;
	long end;
	long position;

	struct mutex mutex;
	long debugger;
	bool status;
};

#if defined(USE_WORKQUEUE)
struct t_ologk_work {
	struct work_struct ologk_work;
	union _uPLogPacket writelogpacket;
};
#endif

struct t_command {
	char *command;
	void (*func)(char *writebuffer);
};

#if defined(USE_MONITOR)
unsigned long mutex_rawdata[MAX_MUTEX_RAWDATA + 1][MAX_MUTEX_RAWDATA_DIGIT] = {{0, },};
#endif

int ops_write_buffer(struct tRingBuffer *buffer,
			byte *data, unsigned long length);
int ops_process_command(struct tRingBuffer *buffer,
			byte *data, unsigned long length);

enum {
	SH_TYPE_PACKET,
	SH_TYPE_COMMAND,
};

enum {
	SH_TYPE,
	SH_IDX_PACKET
};

int (*write_opts[])(struct tRingBuffer *buffer,
			byte *data, unsigned long length)
			= {
				ops_write_buffer,
				ops_process_command,
			};

void set_kperfmon_debugger_function(char *writebuffer);
void process_version_function(char *writebuffer);

struct t_command commands[] = {
	{"kperfmon_debugger", set_kperfmon_debugger_function},
	{"java_version", process_version_function},
	{"nativelib_version", process_version_function},
	{"perfmond_version", process_version_function},
};

struct t_before_print {
	void *pdata;
	int (*func)(char *read_buffer);
	struct list_head list;
};

void CreateBuffer(struct tRingBuffer *buffer,
			unsigned long length);
void DestroyBuffer(struct tRingBuffer *buffer);
void WriteBuffer(struct tRingBuffer *buffer,
			byte *data, unsigned long length);
void GetNext(struct tRingBuffer *buffer);
void ReadBuffer(struct tRingBuffer *buffer,
			byte *data,
			unsigned long *length);
ssize_t kperfmon_write(struct file *filp,
				const char __user *data,
				size_t length,
				loff_t *loff_data);
ssize_t kperfmon_read(struct file *filp,
				char __user *data,
				size_t count,
				loff_t *loff_data);

