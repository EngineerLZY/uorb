/****************************************************************************
 *
 *   Copyright (c) 2012-2015 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#ifndef _uORBTest_UnitTest_hpp_
#define _uORBTest_UnitTest_hpp_
#include "base/orb_defines.h"
#include "base/orb_errno.h"
#include "base/orb_log.h"
#include "uORB.h"
#include "uORBCommon.hpp"
#include "posix_task.h"

#include <unistd.h>

struct orb_test {
	int val;
	hrt_abstime time;
};
ORB_DECLARE(orb_test);
ORB_DECLARE(orb_multitest);

struct orb_test_medium {
	int val;
	hrt_abstime time;
	char junk[64];
};
ORB_DECLARE(orb_test_medium);
ORB_DECLARE(orb_test_medium_multi);
ORB_DECLARE(orb_test_medium_queue);
ORB_DECLARE(orb_test_medium_queue_poll);

struct orb_test_large {
	int val;
	hrt_abstime time;
	char junk[512];
};
ORB_DECLARE(orb_test_large);


namespace uORBTest
{
class UnitTest;
}

class uORBTest::UnitTest
{
public:

	// Singleton pattern
	static uORBTest::UnitTest &instance();
	~UnitTest() {}
	int test();
	template<typename S> int latency_test(orb_id_t T, bool print);

      private:
	UnitTest() : pubsubtest_passed(false), pubsubtest_print(false) {}

	// Disallow copy
	UnitTest(const uORBTest::UnitTest & /*unused*/) = delete;

	static int pubsubtest_threadEntry(int argc, char *argv[]);
	int pubsublatency_main();

	static int pub_test_multi2_entry(int argc, char *argv[]);
	int pub_test_multi2_main();

	volatile bool _thread_should_exit;

	bool pubsubtest_passed;
	bool pubsubtest_print;
	int pubsubtest_res = ORB_OK;

	orb_advert_t _pfd[4]; ///< used for test_multi and test_multi_reversed

	int test_single();

	/* These 3 depend on each other and must be called in this order */
	int test_multi();
	int test_multi_reversed();
	int test_unadvertise();

	int test_multi2();

	/* queuing tests */
	static int test_queue();
	static int pub_test_queue_entry(int argc, char *argv[]);
	int pub_test_queue_main();
	int test_queue_poll_notify();
	volatile int _num_messages_sent = 0;
};

template<typename S>
int uORBTest::UnitTest::latency_test(orb_id_t T, bool print)
{
	ORB_INFO("---------------- LATENCY TEST ------------------");
	S t;
	t.val = 308;
	t.time = hrt_absolute_time();

	orb_advert_t pfd0 = orb_advertise(T, &t);

	if (pfd0 == nullptr) {
		ORB_ERR("orb_advertise failed (%i)", orb_errno);
		return ORB_ERROR;
	}

	char *const args[1] = { nullptr };

	pubsubtest_print = print;
	pubsubtest_passed = false;

	/* sample pub / sub latency */

	// Can't pass a pointer in args, must be a null terminated
	// array of strings because the strings are copied to
	// prevent access if the caller data goes out of scope
	pthread_t pubsub_task = px4_task_spawn_cmd("uorb_latency",
					     SCHED_DEFAULT,
					     SCHED_PRIORITY_MAX,
					     2000,
					     (px4_main_t)&uORBTest::UnitTest::pubsubtest_threadEntry,
					     args);

	/* give the sample task some data */
	while (!pubsubtest_passed) {
		++t.val;
		t.time = hrt_absolute_time();

		if (ORB_OK != orb_publish(T, pfd0, &t)) {
			ORB_ERR("mult. pub0 timing fail");
			return ORB_ERROR;
		}

		/* simulate >800 Hz system operation */
		usleep(1000);
	}

	if (pubsub_task < 0) {
		ORB_ERR("failed launching task");
		return ORB_ERROR;
	}

	orb_unadvertise(pfd0);

	return pubsubtest_res;
}

#endif // _uORBTest_UnitTest_hpp_