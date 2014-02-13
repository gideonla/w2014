#!/usr/bin/python2
import subprocess as sp
import numpy as np
import unittest
import itertools
import Queue
import random

class LockTests(unittest.TestCase):
    def setUp(self):
        self.regular_locks = ['TAS', 'mutex', 'CLH', 'MCS']
        self.delay_tests = [(10, 50000), (20, 25000), (100, 1000), (500, 500)]

    def test_hold_lock(self):
        '''
        First do the non-parametric locks
        '''
        for lock in self.regular_locks:
            for workers in 2**np.arange(5):
                for test_time in 100000*(2**np.arange(4)):
                    out = sp.check_output('./test_main', 'hold_lock', lock, str(workers), str(test_time))
                    if len(out) > 0:
                        raise AssertionError('Failed with lock {0} with {1} workers at {2}us'.format(lock, workers, test_time))

        '''
        Now backoff. 
        '''
        for workers in 2**np.arange(5):
            for test_time in 100000*(2**np.arange(4)):
                for (min_delay, max_delay) in self.delay_tests:
                    out = sp.check_output('./test_main', 'hold_lock', 'backoff', str(min_delay), str(max_delay), str(workers), str(test_time))
                    if len(out) > 0:
                        raise AssertionError('Failed on backoff at {0} with {1} workers at {2}us '.format((min_delay, max_delay), workers, test_time))

        '''
        And A-lock
        '''

        for workers in 2**np.arange(5):
            for test_time in 100000*(2**np.arange(4)):
                for max_threads in (workers + np.arange(1, 4)):
                    out = sp.check_output('./test_main', 'hold_lock', 'Alock', str(min_delay), str(max_delay), str(workers), str(test_time))
                    if len(out) > 0:
                        raise AssertionError('Failed on Alock at max_workers={0} with {1} workers at {2}us '.format(max_threads, workers, test_time))






class QueueTests(unittest.TestCase):
    '''
    Parallel queue tests. Python does very little of the heavy lifting
    here because it involves threading.
    '''
    def test_fast_dqr(self):
        for size in [1, 2, 4, 8, 16, 32]:
            for enq_count in [1, 10, 100, 1000]:
                for delay_mode in [0, 1, 2, 3]:
                    print size, enq_count, delay_mode
                    out = sp.check_output(['./test_main', 'queue_parallel_1', str(size), str(enq_count), str(delay_mode)])
                    if len(out) > 0:
                        raise AssertionError('{0}\nFailed with size={1}, enq_count={2}, delay={3}'.format(
                            out, size, enq_count, delay_mode))


    def test_fast_eqr(self):
        for size in [1, 2, 4, 8, 16, 32]:
            for enq_count in [1, 10, 100, 1000]:
                for delay_mode in [0, 1, 2, 3]:
                    print size, enq_count, delay_mode
                    out = sp.check_output(['./test_main', 'queue_parallel_2', str(size), str(enq_count), str(delay_mode)])
                    if len(out) > 0:
                        raise AssertionError('{0}\nFailed with size={1}, enq_count={2}, delay={3}'.format(
                            out, size, enq_count, delay_mode))

class ParallelTests(unittest.TestCase):
    def test_full_enq(self):
        for n, T, D, distr in itertools.product([1,2,4,8,16], [100, 200], [1,2,4,100], [0,1]):
            out = sp.check_output(['./test_main', 'dispatcher_1', str(T), str(n), str(D), '50', '0', str(distr)])
            nq_cts = out.split()
            if len(nq_cts) != n:
                raise AssertionError('Incorrect return count on n={0}, T={1}, D={2}, distr={3}'.format(n, T, D, distr))
            for t1 in nq_cts:
                if int(t1) != T:
                    raise AssertionError('Incorrect enq count on n={0}, T={1}, D={2}, distr={3}: got {4}'.format(n, T, D, distr, t1))

    def test_lazy_enq(self):
        for n, T, D, distr in itertools.product([2,4,8,16], [100, 200], [1,2,4,100], [0,1]):
            for lazy_cnt in range(1, n, 2):
                out = sp.check_output(['./test_main', 'dispatcher_1', str(T), str(n), str(D), '50', str(lazy_cnt), str(distr)])
                nq_cts = out.split()
                if len(nq_cts) != n:
                    raise AssertionError('Incorrect return count on n={0}, T={1}, D={2}, distr={3}'.format(n, T, D, distr))
                if nq_cts.count(str(T)) != n - lazy_cnt:
                    raise AssertionError('Incorrect busy count on n={0}, T={1}, D={2}, lazy={3}, distr={4}: got {5}'.format(
                        n, T, D, n- lazy_cnt, distr, nq_cts.count(str(T))))

    def test_fingerprint_p(self):
        for n, T, D, distr, seed in itertools.product([1,2,4,8,16], [100, 200], [1,2,4,100], [0,1], [0,1,2]):
            parallel_output = sp.check_output(['../firewall', '2', str(T), str(n), '50', str(distr), str(seed), str(D)]).split()
            serial_output = sp.check_output(['../firewall', '0', str(T), str(n), '50', str(distr), str(seed), str(D)]).strip()
            assert parallel_output[-1] == serial_output

    def test_fingerprint_sq(self):
        for n, T, D, distr, seed in itertools.product([1,2,4,8,16], [100, 200], [1,2,4,100], [0,1], [0,1,2]):
            parallel_output = sp.check_output(['../firewall', '1', str(T), str(n), '50', str(distr), str(seed), str(D)]).split()
            serial_output = sp.check_output(['../firewall', '0', str(T), str(n), '50', str(distr), str(seed), str(D)]).strip()
            assert parallel_output[-1] == serial_output
