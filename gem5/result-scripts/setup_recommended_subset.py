import os
import argparse
from run_config import *

# We now rely on run_config.py's ACCESS_RATES but we override pattern to save time
RECOMMENDED_DATA_PATTERNS = ["0x00"]

def create_run_scripts(command, sim_result_file_path, slurm_job_name):
    script_content = f"{CMD_HEADER}\n{command} > {sim_result_file_path} 2>&1\n"
    if CONTAINER:
        sim_result_file_path_app = sim_result_file_path.replace(BASE_DIR, "/app")
        script_content = f"{CMD_HEADER}\n{CONTAINER_COMMAND} {CONTAINER_CHECK} {CONTAINER_COMMAND} {CONTAINER_LOAD}\n{CONTAINER_COMMAND} run --rm {USER_FLAG} -v {BASE_DIR}:/app  {CONTAINER_IMAGE} \'{command} > {sim_result_file_path_app} 2>&1\'\n"
    
    script_path = f"{BASE_DIR}/run_scripts/{slurm_job_name}.sh"
    with open(script_path, "w") as script_file:
        script_file.write(script_content)
    os.chmod(script_path, 0o755)
    return script_path

def get_recommended_commands():
    run_scripts = []
    test_presets = ["PRAC", "RFM"]
    
    for preset in test_presets:
        # Baseline (Rate 0)
        RESULT_DIR, CFG_FILE, SENDER, RECEIVER, TXN_PERIOD, _ = get_preset_variables(preset, is_noise=False)
        os.makedirs(RESULT_DIR, exist_ok=True)
        os.makedirs(f"{RESULT_DIR}/err", exist_ok=True)
        
        for pattern in RECOMMENDED_DATA_PATTERNS:
            dir_path = make_temp_dir(RESULT_DIR, preset, TXN_PERIOD, MSG_BYTES, pattern, 0)
            result_file = make_res_file(RESULT_DIR, preset, TXN_PERIOD, MSG_BYTES, pattern, 0)
            slurm_job_name = make_stat_str(preset, TXN_PERIOD, MSG_BYTES, pattern, 0)
            os.makedirs(dir_path, exist_ok=True)
            
            command = CMD_STR.format(cfg_file=CFG_FILE, output_dir=dir_path, txn_window=TXN_PERIOD, 
                                   msg_len=MSG_BYTES, data_pattern=pattern, sender=SENDER, 
                                   receiver=RECEIVER, access_rate=0)
            if CONTAINER: command = command.replace(BASE_DIR, "/app")
            script_path = create_run_scripts(command, result_file, slurm_job_name)
            run_scripts.append(f"sh {script_path}")

        # Noise (Using the new diverse rates from run_config.py)
        RESULT_DIR, CFG_FILE, SENDER, RECEIVER, TXN_PERIOD, RATES = get_preset_variables(preset, is_noise=True)
        os.makedirs(RESULT_DIR, exist_ok=True)
        os.makedirs(f"{RESULT_DIR}/err", exist_ok=True)
        
        for rate in RATES:
            for pattern in RECOMMENDED_DATA_PATTERNS:
                dir_path = make_temp_dir(RESULT_DIR, preset, TXN_PERIOD, MSG_BYTES, pattern, rate)
                result_file = make_res_file(RESULT_DIR, preset, TXN_PERIOD, MSG_BYTES, pattern, rate)
                slurm_job_name = make_stat_str(preset, TXN_PERIOD, MSG_BYTES, pattern, rate)
                os.makedirs(dir_path, exist_ok=True)
                
                attack_period = TXN_PERIOD * MSG_BYTES * 8 * 1.5
                noise_generator = f"{BASE_DIR}/attack-binaries/mr_noise"
                command = NOISE_CMD_STR.format(cfg_file=CFG_FILE, output_dir=dir_path, txn_window=TXN_PERIOD,
                                             msg_len=MSG_BYTES, data_pattern=pattern, sender=SENDER,
                                             receiver=RECEIVER, attack_period=attack_period,
                                             noise_generator=noise_generator, access_rate=rate)
                if CONTAINER: command = command.replace(BASE_DIR, "/app")
                script_path = create_run_scripts(command, result_file, slurm_job_name)
                run_scripts.append(f"sh {script_path}")
                
    return run_scripts

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-c", "--container", action="store_true")
    parser.add_argument("-cc", "--container_command", default="podman")
    args = parser.parse_args()

    # Need to override some flags from run_config to be consistent
    CONTAINER = args.container
    CONTAINER_COMMAND = args.container_command
    USER_FLAG = f"--user {os.getuid()}:{os.getgid()}"

    os.makedirs(f"{BASE_DIR}/run_scripts", exist_ok=True)
    cmds = get_recommended_commands()
    
    with open("run_subset.sh", "w") as f:
        for cmd in cmds:
            f.write(f"{cmd}\n")
    os.chmod("run_subset.sh", 0o755)
