
#include <string>
#include <time.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <univalue.h>
#include <stdio.h>
#include "Util.h"

using namespace std;

std::string addressToStr(const struct sockaddr *sockaddr, socklen_t socklen)
{
        char name[1025] = "";
        if (!getnameinfo(sockaddr, socklen, name, sizeof(name), NULL, 0, NI_NUMERICHOST))
            return std::string(name);

	return string("");
}

std::string formatTime(const std::string& fmt, time_t t)
{
	// convert unix seconds-since-epoch to split-out struct
	struct tm tm;
	gmtime_r(&t, &tm);

	// build formatted time string
	char timeStr[fmt.size() + 256];
	strftime(timeStr, sizeof(timeStr), fmt.c_str(), &tm);

	return string(timeStr);
}

std::string isoTimeStr(time_t t)
{
	return formatTime("%FT%TZ", t);
}

bool readJsonFile(const std::string& filename, UniValue& jval)
{
	jval.clear();

        FILE *f = fopen(filename.c_str(), "r");
	if (!f)
		return false;

        string jdata;

        char buf[4096];
        while (!feof(f)) {
                int bread = fread(buf, 1, sizeof(buf), f);
		if (ferror(f)) {
			fclose(f);
			return false;
		}

                string s(buf, bread);
                jdata += s;
        }

	if (ferror(f)) {
		fclose(f);
		return false;
	}
        fclose(f);

	if (!jval.read(jdata))
		return false;

        return true;
}

int write_pid_file(const std::string& pidFn)
{
	char str[32], *s;
	size_t bytes;
	int fd;
	struct flock lock;
	int err;
	const char *pid_fn = pidFn.c_str();

	/* build file data */
	sprintf(str, "%u\n", (unsigned int) getpid());

	/* open non-exclusively (works on NFS v2) */
	fd = open(pid_fn, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		err = errno;

		syslog(LOG_DAEMON|LOG_ERR, "Cannot open PID file %s: %s",
			 pid_fn, strerror(err));
		return -err;
	}

	/* lock */
	memset(&lock, 0, sizeof(lock));
	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	if (fcntl(fd, F_SETLK, &lock) != 0) {
		err = errno;
		if (err == EAGAIN) {
			syslog(LOG_DAEMON|LOG_ERR, "PID file %s is already locked",
				 pid_fn);
		} else {
			syslog(LOG_DAEMON|LOG_ERR, "Cannot lock PID file %s: %s",
				 pid_fn, strerror(err));
		}
		close(fd);
		return -err;
	}

	/* write file data */
	bytes = strlen(str);
	s = str;
	while (bytes > 0) {
		ssize_t rc = write(fd, s, bytes);
		if (rc < 0) {
			err = errno;
			syslog(LOG_DAEMON|LOG_ERR, "PID number write failed: %s",
				 strerror(err));
			goto err_out;
		}

		bytes -= rc;
		s += rc;
	}

	/* make sure file data is written to disk */
	if (fsync(fd) < 0) {
		err = errno;
		syslog(LOG_DAEMON|LOG_ERR, "PID file fsync failed: %s", strerror(err));
		goto err_out;
	}

	return fd;

err_out:
	unlink(pid_fn);
	close(fd);
	return -err;
}

