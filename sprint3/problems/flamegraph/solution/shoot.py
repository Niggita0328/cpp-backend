import argparse
import subprocess
import time
import random
import shlex
import signal
 
RANDOM_LIMIT = 1000
SEED = 123456789
random.seed(SEED)
 
AMMUNITION = [
    'localhost:8080/api/v1/maps/map1',
    'localhost:8080/api/v1/maps'
]
 
SHOOT_COUNT = 100
COOLDOWN = 0.1
 
def start_server():
    parser = argparse.ArgumentParser()
    parser.add_argument('server', type=str)
    return parser.parse_args().server
 
 
def run(command, output=None):
    process = subprocess.Popen(shlex.split(command), stdout=output, stderr=subprocess.DEVNULL)
    return process
 
 
def stop(process, wait=False):
    if process.poll() is None and wait:
        process.wait()
    process.terminate()
 
 
def shoot(ammo):
    hit = run('curl ' + ammo, output=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(hit, wait=True)
 
 
def make_shots():
    for _ in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
    print('Shooting complete')
 
 
server = run(start_server())
perf_process = run(f'perf record -g -p {server.pid} -o perf.data') 
make_shots()
perf_process.send_signal(signal.SIGINT) # Используем SIGINT
perf_process.wait()
 
time.sleep(1)
print('Job done')
stop(server)
 
with open('graph.svg', 'w') as svg_file:
    perf_command = 'perf script -i perf.data'
    perf_script = subprocess.Popen(shlex.split(perf_command), stdout=subprocess.PIPE, text=True)
 
    stackcollapse_cmd = ['./FlameGraph/stackcollapse-perf.pl']
    p1 = subprocess.Popen(stackcollapse_cmd, stdin=perf_script.stdout, stdout=subprocess.PIPE, text=True)
 
    flamegraph_cmd = ['./FlameGraph/flamegraph.pl']
    p2 = subprocess.Popen(flamegraph_cmd, stdin=p1.stdout, stdout=svg_file, text=True)
 
    # Закрываем stdout первого процесса, чтобы второй мог завершиться
    p1.stdout.close()
    # Ожидаем завершения второго процесса в пайпе
    p2.wait()