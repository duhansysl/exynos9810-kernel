#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/setup.h>

enum {
	FLAG_DELETE = 0,
	FLAG_REPLACE,
};

static char new_command_line[COMMAND_LINE_SIZE];

static int cmdline_proc_show(struct seq_file *m, void *v)
{
	seq_puts(m, new_command_line);
	seq_putc(m, '\n');
	return 0;
}

static int cmdline_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, cmdline_proc_show, NULL);
}

static const struct file_operations cmdline_proc_fops = {
	.open		= cmdline_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#ifdef CONFIG_PROC_SPOOF_CMDLINE
static int process_flag(int replace, const char *flag, const char *new_var)
{
	char *start_flag, *end_flag, *next_flag;
	char *last_char = new_command_line + COMMAND_LINE_SIZE;
	size_t rest_len, flag_len, cmd_len, var_len, nvar_len;
	int ret = 0;

	/* Ensure all instances of a flag are removed */
	while ((start_flag = strnstr(new_command_line, flag, COMMAND_LINE_SIZE))) {
		end_flag = strnchr(start_flag, last_char - start_flag, ' ');

		/* this may happend when copied cmdline is filled up fully */
		if (end_flag > last_char)
			end_flag = last_char;

		cmd_len = strlen(new_command_line);
		if (unlikely(cmd_len > COMMAND_LINE_SIZE))
			break;

		next_flag = end_flag + 1;
		rest_len = (size_t)(last_char - end_flag);
		flag_len = (size_t)(end_flag - start_flag);

		if (replace) {
			if (!new_var)
				break;

			nvar_len = strlen(new_var);
			var_len = flag_len - strlen(flag);

			// sanity check
			if (nvar_len > var_len &&
			    (cmd_len + (nvar_len - var_len)) > COMMAND_LINE_SIZE)
				break;
		}

		if (rest_len)
			memmove(start_flag, next_flag, rest_len);

		memset(last_char - flag_len, '\0', flag_len);

		ret++;

		/* remove token first, insert at the last */
		if (replace) {
			cmd_len = strlen(new_command_line);
			if (unlikely(cmd_len > COMMAND_LINE_SIZE))
				break;

			sprintf(new_command_line + cmd_len, " %s%s", flag, new_var);

			// TODO: restrict rest space clean

			/* avoid dead loop */
			break;
		}
	}

	return ret;
}
#endif

static int __init proc_cmdline_init(void)
{
	memcpy(new_command_line, saved_command_line,
		min((size_t)COMMAND_LINE_SIZE, strlen(saved_command_line)));

#ifdef CONFIG_PROC_SPOOF_CMDLINE
	/*
	 * Remove various flags from command line seen by userspace in order to
	 * pass SafetyNet CTS check.
	 */
	process_flag(FLAG_REPLACE, "androidboot.verifiedbootstate=", "green"); // Play Integrity API / SafetyNet
	process_flag(FLAG_REPLACE, "androidboot.warranty_bit=", "0"); // Bootloader status and Knox
	process_flag(FLAG_REPLACE, "androidboot.fmp_config=", "1"); // Samsung Knox FMP / FIPS
#endif

	proc_create("cmdline", 0, NULL, &cmdline_proc_fops);
	return 0;
}
fs_initcall(proc_cmdline_init);
