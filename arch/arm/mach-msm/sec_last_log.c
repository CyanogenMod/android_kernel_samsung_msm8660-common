#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/stat.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>

#ifdef CONFIG_SEC_LOG_LAST_KMSG
char *last_kmsg_buffer;
unsigned last_kmsg_size;

static ssize_t sec_log_read_old(struct file *file, char __user *buf,
				size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count;

	if (pos >= last_kmsg_size)
		return 0;

	count = min(len, (size_t) (last_kmsg_size - pos));
	if (copy_to_user(buf, last_kmsg_buffer + pos, count))
		return -EFAULT;

	*offset += count;
	return count;
}

static const struct file_operations last_kmsg_file_ops = {
	.owner = THIS_MODULE,
	.read = sec_log_read_old,
};

static int __init sec_log_late_init(void)
{
	struct proc_dir_entry *entry;

	if (last_kmsg_buffer == NULL)
		return 0;

	entry = create_proc_entry("last_kmsg", S_IFREG | S_IRUGO, NULL);
	if (!entry) {
		pr_err("%s: failed to create proc entry\n", __func__);
		return 0;
	}

	entry->proc_fops = &last_kmsg_file_ops;
	entry->size = last_kmsg_size;
	return 0;
}

late_initcall(sec_log_late_init);
#endif