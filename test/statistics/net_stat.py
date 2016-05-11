#!/usr/bin/python
#-*- coding: utf8 -*-

import sys, os, copy, time, subprocess, threading

# 测试参数维度
nodelay = [False, True]
package_size = [64, 4096]
conn = [1, 10, 100, 1000]
pipeline = [1, 100, 1000]
recv_buffer = [16]   #(KBytes)
threads = [1, 2, 3, 4]

envs = {
        'no_delay':nodelay,
        'package_size':package_size,
        'conn':conn,
        'pipeline':pipeline,
        'recv_buffer':recv_buffer,
        'threads':threads
    }

env_count = len(envs.keys())

env_results = []  # [env + result]

print '------------ start network statistics ------------'

def start_server(env):
    command = '../test/bmserver.t %s %s %s %s > log' % (repr(env['package_size']), repr(env['no_delay'] and 1 or 0), repr(env['recv_buffer']), repr(env['threads']))
    print 'command: %s' % command
    return subprocess.Popen(command, shell = True)

def start_client(env):
    pass

def kill(p, seconds_delay):
    time.sleep(seconds_delay)
    print 'kill server'
    p.kill()
    print 'kill server done'

def run_test(env):
    print '----------- run by %s -----------' % env
    server = start_server(env)
    #print dir(server)
    time.sleep(0.5)

    client = start_client(env)
    time.sleep(2)

    server.kill()
    #client.kill()

    print 'will read'
    fout = open('log', 'r')
    while True:
        out = fout.readline()
        if not out:
            break
        print out
    fout.close()
    print 'read done'

    return res_str

# parse the client output string, extract the throughput float number (MB).
def parse(res_str):
    pass

def recursive_test(env = {}, deep = 0):
    if deep == env_count:
        res_str = run_test(env)
        env['result'] = parse(res_str)
        env_results.append(env)
        print env
        return 
    
    key = envs.keys()[deep]
    for param in envs[key]:
        new_env = copy.deepcopy(env)
        new_env[key] = param
        recursive_test(new_env, deep + 1)

def main():
    recursive_test()
    print 'result count:', len(env_results)

if __name__ == '__main__':
    main()

