import csv
import random

# Định nghĩa header chuẩn
HEADER = ["timestamp", "thread_id", "thread_type", "action", "target_resource", "resource_type", "duration_ms", "timeout_threshold"]

def write_csv(filename, data):
    with open(filename, mode='w', newline='') as file:
        writer = csv.writer(file)
        writer.writerow(HEADER)
        writer.writerows(data)

# ---------------------------------------------------------
# Case 16: Tuyến đường tử thần (Deep Tree Traversal)
# ---------------------------------------------------------
def generate_case_16():
    data = []
    # Giai đoạn 1: 100 Threads khóa 100 Resources
    for i in range(1, 101):
        data.append([i * 10, f"P{i}", "WORKER", "LOCK", f"R{i}", "Mutex", "null", "null"])
    
    # Giai đoạn 2: P(i) xin R(i+1) tạo thành chuỗi 100 node
    for i in range(1, 100):
        data.append([1000 + i * 10, f"P{i}", "WORKER", "LOCK", f"R{i+1}", "Mutex", "null", "null"])
    
    write_csv("case_16_deep_tree.csv", data)

# ---------------------------------------------------------
# Case 17: Chu trình siêu lớn (Massive Cycle)
# ---------------------------------------------------------
def generate_case_17():
    data = []
    # Giai đoạn 1: 50 Threads khóa 50 Resources
    for i in range(1, 51):
        data.append([i * 10, f"P{i}", "WORKER", "LOCK", f"R{i}", "Mutex", "null", "null"])
    
    # Giai đoạn 2: P1 chờ P2, P2 chờ P3... P50 chờ P1
    for i in range(1, 51):
        target = 1 if i == 50 else i + 1
        data.append([1000 + i * 10, f"P{i}", "WORKER", "LOCK", f"R{target}", "Mutex", "null", "null"])
    
    write_csv("case_17_massive_cycle.csv", data)

# ---------------------------------------------------------
# Case 18: Đồ thị dày đặc (Dense Graph Stress Test)
# ---------------------------------------------------------
def generate_case_18():
    data = []
    resources = [f"R{i}" for i in range(1, 101)] # 100 Resources
    
    # 50 Threads (10 UI, 40 WORKER)
    threads = [(f"P{i}", "UI" if i <= 10 else "WORKER") for i in range(1, 51)]
    
    timestamp = 0
    # Mỗi thread lấy ngẫu nhiên 5 resource
    for t_id, t_type in threads:
        timeout = 5000 if t_type == "UI" else "null"
        held_res = random.sample(resources, 5)
        for r in held_res:
            timestamp += 5
            data.append([timestamp, t_id, t_type, "LOCK", r, "Mutex", "null", timeout])
            
    # Mỗi thread xin ngẫu nhiên 5 resource khác để tạo "rừng" chu trình (E >= 200)
    for t_id, t_type in threads:
        timeout = 5000 if t_type == "UI" else "null"
        req_res = random.sample(resources, 5)
        for r in req_res:
            timestamp += 5
            data.append([timestamp, t_id, t_type, "LOCK", r, "Mutex", "null", timeout])

    write_csv("case_18_dense_graph.csv", data)

# ---------------------------------------------------------
# Case 19: Heavy Traffic (Không có Deadlock)
# ---------------------------------------------------------
def generate_case_19():
    data = []
    timestamp = 0
    # 10,000 sự kiện LOCK/UNLOCK liên tiếp
    for i in range(5000): # 5000 LOCK + 5000 UNLOCK
        t_id = f"P{random.randint(1, 20)}"
        r_id = f"R{random.randint(1, 50)}"
        timestamp += 2
        # Xin khóa có duration ngắn (20ms) để không bao giờ kẹt vĩnh viễn
        data.append([timestamp, t_id, "WORKER", "LOCK", r_id, "Message_Queue", 20, "null"])
        
    write_csv("case_19_heavy_traffic.csv", data)

# ---------------------------------------------------------
# Case 20: Random Walk (Mô phỏng App thực tế 1 giờ)
# ---------------------------------------------------------
def generate_case_20():
    data = []
    timestamp = 0
    # Mô phỏng ~2000 sự kiện ngẫu nhiên trong 1 giờ
    for i in range(2000):
        timestamp += random.randint(10, 2000) # Giãn cách ngẫu nhiên
        is_ui = random.random() < 0.2 # 20% UI, 80% WORKER
        
        t_id = f"P{random.randint(1, 5)}" if is_ui else f"P{random.randint(6, 25)}"
        t_type = "UI" if is_ui else "WORKER"
        timeout = 5000 if is_ui else "null"
        r_id = f"R{random.randint(1, 30)}"
        duration = random.choice(["null", 50, 100, 500])
        
        data.append([timestamp, t_id, t_type, "LOCK", r_id, "Mutex", duration, timeout])
        
    write_csv("case_20_random_walk.csv", data)

# Thực thi
generate_case_16()
generate_case_17()
generate_case_18()
generate_case_19()
generate_case_20()
print("Đã sinh xong 5 file CSV!")