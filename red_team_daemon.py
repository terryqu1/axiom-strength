import time
import os
import urllib.request
import urllib.error
import json

# Sovereign Configuration
TARGET_FILE = "gaussianProcess.cpp"
OLLAMA_URL = "http://127.0.0.1:11434/api/generate"
DIRECTIVE_FILE = "red_team_directive.txt"
LLM_MODEL = "deepseek-coder-v2:16b" 

def load_directive():
    with open(DIRECTIVE_FILE, 'r') as f:
        return f.read()

def get_file_modification_time(filepath):
    try:
        return os.path.getmtime(filepath)
    except FileNotFoundError:
        return 0

def execute_red_team_analysis(cpp_code, directive):
    payload = {
        "model": LLM_MODEL,
        "prompt": f"{directive}\n\nTARGET C++ ARCHITECTURE:\n{cpp_code}",
        "stream": True
    }
    
    data = json.dumps(payload).encode('utf-8')
    req = urllib.request.Request(OLLAMA_URL, data=data, headers={'Content-Type': 'application/json'})
    
    print("=== RED TEAM SOCRATIC CRITIQUE ===")
    
    try:
        with urllib.request.urlopen(req) as response:
            for line in response:
                if line:
                    chunk = json.loads(line.decode('utf-8'))
                    if 'response' in chunk:
                        print(chunk['response'], end='', flush=True)
        print("\n==================================\n")
    except urllib.error.HTTPError as e:
        error_body = e.read().decode()
        print(f"\n[!] API Rejected Request: HTTP {e.code} - {error_body}\n")
    except urllib.error.URLError as e:
        print(f"\n[!] Network failure: {e.reason}. Is Ollama running on port 11434?\n")
    except Exception as e:
        print(f"\n[!] Local inference failed: {e}\n")

def monitor_workspace(TARGET_FILE):
    if not os.path.exists(TARGET_FILE):
        open(TARGET_FILE, 'w').close()
    if not os.path.exists(DIRECTIVE_FILE):
        print(f"[!] {DIRECTIVE_FILE} is missing. Create it before running.")
        return

    print(f"Monitoring {TARGET_FILE} for structural vulnerabilities...")
    last_mtime = get_file_modification_time(TARGET_FILE)
    directive = load_directive()

    while True:
        time.sleep(2)
        current_mtime = get_file_modification_time(TARGET_FILE)
        
        if current_mtime > last_mtime:
            last_mtime = current_mtime
            
            os.system('clear')
            print(f"--- COMPILE TRIGGER DETECTED: ANALYZING {TARGET_FILE} ---\n")
            print(f"Pinging local M1 Max GPU ({LLM_MODEL})...\n")
            
            with open(TARGET_FILE, 'r') as f:
                cpp_code = f.read()
                
            execute_red_team_analysis(cpp_code, directive)
            
            print("Awaiting next save state...")

if __name__ == "__main__":
    monitor_workspace(TARGET_FILE)