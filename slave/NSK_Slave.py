import tkinter as tk
from tkinter import ttk, filedialog
import json
import threading
import queue
import time
import socket
from datetime import datetime
import os
try:
	import serial
	from serial.tools import list_ports
except Exception:
	serial = None
	list_ports = None

root = tk.Tk()
root.title("Protocol Slave Simulator")

# 顶部：配置文件选择
config_frame = ttk.LabelFrame(root, text="配置文件")
config_frame.pack(fill="x", padx=10, pady=5)

ttk.Label(config_frame, text="配置:").pack(side="left", padx=5)
current_config_file = tk.StringVar(value="")
config_combo = ttk.Combobox(config_frame, textvariable=current_config_file, width=20)
config_combo.pack(side="left", padx=5)

browse_btn = ttk.Button(config_frame, text="浏览...")
browse_btn.pack(side="left", padx=5)

load_btn = ttk.Button(config_frame, text="加载配置")
load_btn.pack(side="left", padx=5)

# 连接配置
conn_frame = ttk.LabelFrame(root, text="连接配置")
conn_frame.pack(fill="x", padx=10, pady=5)

ttk.Label(conn_frame, text="模式:").pack(side="left")
mode_combo = ttk.Combobox(conn_frame, values=["串口", "网络"], width=6)
mode_combo.set("串口")
mode_combo.pack(side="left", padx=5)

serial_frame = ttk.Frame(conn_frame)
tcp_frame = ttk.Frame(conn_frame)

# 串口子框
ttk.Label(serial_frame, text="串口:").pack(side="left")
port_combo = ttk.Combobox(serial_frame, values=[], width=12)
port_combo.pack(side="left", padx=5)
refresh_btn = ttk.Button(serial_frame, text="扫描")
refresh_btn.pack(side="left", padx=5)

ttk.Label(serial_frame, text="波特率:").pack(side="left")
baud_entry = ttk.Entry(serial_frame, width=8)
baud_entry.insert(0, "115200")
baud_entry.pack(side="left", padx=5)

# 网络子框
ttk.Label(tcp_frame, text="IP:").pack(side="left")
ip_entry = ttk.Entry(tcp_frame, width=12)
ip_entry.insert(0, "127.0.0.1")
ip_entry.pack(side="left", padx=5)

ttk.Label(tcp_frame, text="端口:").pack(side="left")
tcp_port_entry = ttk.Entry(tcp_frame, width=6)
tcp_port_entry.insert(0, "9000")
tcp_port_entry.pack(side="left", padx=5)

# 初始显示串口子框
serial_frame.pack(side="left")

connect_btn = ttk.Button(conn_frame, text="连接")
connect_btn.pack(side="left", padx=5)

def on_save_config():
	try:
		config_file = current_config_file.get().strip()
		if not config_file:
			config_file = "cmd.json"
		
		data = {}
		if os.path.exists(config_file):
			try:
				with open(config_file, "r", encoding="utf-8") as f:
					data = json.load(f)
			except Exception:
				pass
		
		# 构建或更新命令列表
		cmds = data.get("commands", [])
		
		# 如果没有现有命令，从当前界面创建
		if len(cmds) == 0:
			for entry in cmd_entries:
				cmd_info = commands[entry["index"]]
				max_len = entry.get("resp_data_len")
				val = entry.get("resp_hex").get()
				cmds.append({
					"name": cmd_info.get("name", ""),
					"cmd_id": cmd_info.get("cmd_id", ""),
					"resp_id": cmd_info.get("resp_id", ""),
					"resp_data_len": cmd_info.get("resp_data_len", 0),
					"resp_hex": sanitize_hex_input(val, max_len if isinstance(max_len, int) else None)
				})
		else:
			# 更新现有命令的 resp_hex
			for i, entry in enumerate(cmd_entries):
				if i < len(cmds):
					max_len = entry.get("resp_data_len")
					val = entry.get("resp_hex").get()
					cmds[i]["resp_hex"] = sanitize_hex_input(val, max_len if isinstance(max_len, int) else None)
		
		data["commands"] = cmds
		with open(config_file, "w", encoding="utf-8") as f:
			json.dump(data, f, ensure_ascii=False, indent=2)
		log_message(f"配置已保存到 {config_file}")
	except Exception as e:
		log_message(f"保存失败: {e}")

save_btn = ttk.Button(conn_frame, text="保存配置", command=on_save_config)
save_btn.pack(side="right", padx=5)

# 中部：命令配置（根据 cmd.json 动态生成）- 增加滚动，单列布局
cmd_container = ttk.LabelFrame(root, text="命令配置")
cmd_container.pack(fill="both", expand=True, padx=10, pady=5)

# 使用 Canvas + Scrollbar 实现可滚动区域
cmd_canvas = tk.Canvas(cmd_container, highlightthickness=0)
cmd_scrollbar = ttk.Scrollbar(cmd_container, orient="vertical", command=cmd_canvas.yview)
cmd_canvas.configure(yscrollcommand=cmd_scrollbar.set)
cmd_canvas.pack(side="left", fill="both", expand=True)
cmd_scrollbar.pack(side="right", fill="y")

# 固定一个较友好的显示高度，避免挤占日志区域
cmd_canvas.configure(height=300, yscrollincrement=20)

# 真正放控件的内层 Frame
inner_frame = ttk.Frame(cmd_canvas)
cmd_canvas.create_window((0, 0), window=inner_frame, anchor="nw")

# 鼠标滚轮支持（macOS/Windows/Linux）
def _on_mousewheel(event):
	try:
		delta = -1 if event.delta > 0 else 1
		if abs(event.delta) >= 120:
			delta = -int(event.delta / 120)
		cmd_canvas.yview_scroll(delta, "units")
	except Exception:
		pass

def _bind_wheel(_):
	cmd_canvas.bind_all("<MouseWheel>", _on_mousewheel)

def _unbind_wheel(_):
	cmd_canvas.unbind_all("<MouseWheel>")

cmd_canvas.bind("<Enter>", _bind_wheel)
cmd_canvas.bind("<Leave>", _unbind_wheel)
cmd_canvas.bind("<Button-4>", lambda e: cmd_canvas.yview_scroll(-1, "units"))
cmd_canvas.bind("<Button-5>", lambda e: cmd_canvas.yview_scroll(1, "units"))

_scrollregion_scheduled = {"flag": False}

def _update_scrollregion(event=None):
	if _scrollregion_scheduled["flag"]:
		return
	_scrollregion_scheduled["flag"] = True
	def _do():
		try:
			cmd_canvas.configure(scrollregion=cmd_canvas.bbox("all"))
		finally:
			_scrollregion_scheduled["flag"] = False
	root.after_idle(_do)

inner_frame.bind("<Configure>", _update_scrollregion)

# 保存每条命令的设置变量
cmd_settings = {}
cmd_entries = []  # 与 commands 顺序一致，用于保存回写 JSON
commands = []  # 当前加载的命令列表

def sanitize_hex_input(s: str, max_len: int | None) -> str:
	if s is None:
		s = ""
	hex_chars = "0123456789abcdefABCDEF"
	filtered = "".join(ch for ch in s if ch in hex_chars)
	filtered = filtered.upper()
	usable_len = len(filtered) - (len(filtered) % 2)
	if usable_len < 0:
		usable_len = 0
	filtered = filtered[:usable_len]
	pairs = [filtered[i:i+2] for i in range(0, len(filtered), 2)]
	if isinstance(max_len, int) and max_len > 0:
		pairs = pairs[:max_len]
	return " ".join(pairs)

def scan_json_files():
	"""扫描当前目录下的所有 JSON 文件"""
	try:
		files = [f for f in os.listdir(".") if f.endswith(".json")]
		config_combo["values"] = files
	except Exception as e:
		log_message(f"扫描 JSON 文件失败: {e}")

def browse_config_file():
	"""浏览并选择配置文件"""
	filename = filedialog.askopenfilename(
		title="选择配置文件",
		filetypes=[("JSON files", "*.json"), ("All files", "*.*")],
		initialdir="."
	)
	if filename:
		current_config_file.set(filename)
		log_message(f"已选择配置文件: {filename}")

def clear_command_ui():
	"""清除当前所有命令配置界面"""
	for widget in inner_frame.winfo_children():
		widget.destroy()
	cmd_settings.clear()
	cmd_entries.clear()

def load_config_file():
	"""加载选中的配置文件"""
	global commands
	config_file = current_config_file.get().strip()
	if not config_file:
		log_message("请先选择配置文件")
		return
	
	if not os.path.exists(config_file):
		log_message(f"文件不存在: {config_file}")
		return
	
	try:
		with open(config_file, "r", encoding="utf-8") as f:
			data = json.load(f)
			commands = data.get("commands", [])
		
		# 清除旧的界面
		clear_command_ui()
		
		# 重新创建命令配置界面
		create_command_ui()
		
		log_message(f"已从 {config_file} 加载 {len(commands)} 条命令配置")
	except Exception as e:
		log_message(f"加载配置文件失败: {e}")

def create_command_ui():
	"""根据 commands 列表创建命令配置界面"""
	for idx, cmd in enumerate(commands):
		name = cmd.get("name", f"CMD {idx+1}")
		cmd_id = cmd.get("cmd_id", "")
		resp_id = cmd.get("resp_id", "")

		row = idx
		col = 0

		frame = ttk.Frame(inner_frame)
		frame.grid(row=row, column=col, padx=8, pady=6, sticky="ew")
		frame.columnconfigure(0, weight=0)
		frame.columnconfigure(1, weight=1)
		frame.columnconfigure(2, weight=0)
		frame.columnconfigure(3, weight=1)

		# 顶部信息行：名称/CMD/回复ID/长度（一行显示）
		resp_len = cmd.get("resp_data_len", "-")
		info_text = f"{name}   [命令: {cmd_id}    回复: {resp_id}    长度: {resp_len}]"
		ttk.Label(frame, text=info_text).grid(row=0, column=0, columnspan=4, padx=6, pady=(2, 6), sticky="w")

		# 延迟时间（毫秒）
		ttk.Label(frame, text="延迟(ms):").grid(row=1, column=0, padx=6, pady=4, sticky="w")
		delay_var = tk.IntVar(value=0)
		delay_entry = ttk.Entry(frame, textvariable=delay_var, width=8)
		delay_entry.grid(row=1, column=1, padx=6, pady=4, sticky="w")

		# 几次后开始回复（从第 N 次请求起才回复）
		ttk.Label(frame, text="几次后开始回复:").grid(row=1, column=2, padx=12, pady=4, sticky="w")
		start_after_var = tk.IntVar(value=0)
		start_after_spin = tk.Spinbox(frame, from_=0, to=100, textvariable=start_after_var, width=6)
		start_after_spin.grid(row=1, column=3, padx=6, pady=4, sticky="w")

		# 回复内容（HEX，可编辑）
		given_hex = cmd.get("resp_hex", "")
		max_len_bytes = resp_len if isinstance(resp_len, int) else None
		sanitized_hex = sanitize_hex_input(given_hex, max_len_bytes)
		if not sanitized_hex and isinstance(resp_len, int) and resp_len > 0:
			sanitized_hex = " ".join(["00"] * resp_len)
		resp_hex_var = tk.StringVar(value=sanitized_hex)
		ttk.Label(frame, text="回复内容(HEX):").grid(row=2, column=0, padx=6, pady=4, sticky="w")
		resp_hex_entry = ttk.Entry(frame, textvariable=resp_hex_var, width=50)
		resp_hex_entry.grid(row=2, column=1, columnspan=3, padx=6, pady=4, sticky="ew")

		def _on_hex_focus_out(event=None, var=resp_hex_var, max_len=max_len_bytes):
			var.set(sanitize_hex_input(var.get(), max_len))

		resp_hex_entry.bind("<FocusOut>", _on_hex_focus_out)

		# 存储变量引用，后续逻辑可读取这些值
		cmd_settings[cmd_id] = {
			"name": name,
			"delay_ms": delay_var,
			"start_after": start_after_var,
			"resp_id": resp_id,
			"resp_data_len": resp_len,
			"resp_hex": resp_hex_var,
		}

		cmd_entries.append({
			"index": idx,
			"resp_hex": resp_hex_var,
			"resp_data_len": resp_len,
		})
	
	# 更新滚动区域
	_update_scrollregion()

# 初始扫描 JSON 文件但不加载
scan_json_files()
print("程序已启动，请选择配置文件并点击'加载配置'按钮")

# 底部：日志区域
log_text = tk.Text(root, height=10, width=50)
log_text.pack(fill="both", expand=True, padx=10, pady=5)

########################
# 串口管理与日志
########################

log_queue = queue.Queue()

def log_message(msg: str):
	"""将消息和当前时间戳一起放入队列"""
	now = datetime.now()
	ts = now.strftime('%H:%M:%S') + f".{now.microsecond // 1000:03d}"
	log_queue.put((ts, msg))

def append_log(ts: str, msg: str):
	"""将带时间戳的消息显示到日志区域"""
	try:
		log_text.insert("end", f"[{ts}] {msg}\n")
		log_text.see("end")
	except Exception:
		pass

class SerialManager:
	def __init__(self):
		self.ser = None
		self.reader = None
		self.stop_event = threading.Event()

	def is_connected(self):
		return self.ser is not None and self.ser.is_open

	def connect(self, port: str, baud: int):
		if serial is None:
			log_message("未安装 pyserial，无法连接串口。请先安装：pip install pyserial")
			return False
		try:
			self.ser = serial.Serial(port=port, baudrate=baud, timeout=0.2)
			self.stop_event.clear()
			self.reader = threading.Thread(target=self._read_loop, daemon=True)
			self.reader.start()
			return True
		except Exception as e:
			log_message(f"连接失败: {e}")
			self.ser = None
			return False

	def disconnect(self):
		try:
			self.stop_event.set()
			if self.reader and self.reader.is_alive():
				self.reader.join(timeout=1.0)
		except Exception:
			pass
		try:
			if self.ser and self.ser.is_open:
				self.ser.close()
		except Exception:
			pass
		self.ser = None
		self.reader = None

	def _read_loop(self):
		while not self.stop_event.is_set():
			try:
				if self.ser and self.ser.is_open:
					data = self.ser.read(1024)
					if data:
						# 显示原始十六进制和ASCII解码
						hex_str = " ".join(f"{b:02X}" for b in data)
						ascii_str = data.decode('ascii', errors='ignore')
						log_message(f"RX [HEX]: {hex_str}")
						log_message(f"RX [ASCII]: {ascii_str}")
						# 处理接收到的数据并发送响应
						process_received_data(data, self.send)
				time.sleep(0.02)
			except Exception as e:
				log_message(f"读取错误: {e}")
				time.sleep(0.2)
				break
	
	def send(self, data: bytes):
		"""发送数据到串口"""
		try:
			if self.ser and self.ser.is_open:
				self.ser.write(data)
		except Exception as e:
			log_message(f"串口发送错误: {e}")

serial_mgr = SerialManager()

# 命令计数器：记录每个命令ID被接收了多少次
cmd_counters = {}
# 连续计数器：记录连续收到同一命令的次数
last_cmd_id = None
consecutive_count = 0

def calculate_xor(data_str: str) -> str:
	"""
	计算字符串的XOR校验值
	data_str: 包含 @ + 数据 + * 的完整字符串（ASCII字符）
	返回: 两位十六进制字符串
	"""
	xor_val = 0
	for ch in data_str:
		xor_val ^= ord(ch)
	return f"{xor_val:02X}"

def parse_protocol_frame(data: bytes) -> tuple:
	"""
	解析协议帧：@<HEX_DATA>*<XOR>
	返回: (命令ID字符串, 完整十六进制数据) 或 (None, None)
	"""
	try:
		# 转换为字符串
		frame_str = data.decode('ascii', errors='ignore')
		
		# 查找帧头和帧尾
		start_idx = frame_str.find('@')
		end_idx = frame_str.find('*')
		
		if start_idx == -1 or end_idx == -1 or end_idx <= start_idx:
			return None, None
		
		# 检查*后面是否有至少2位校验值
		if len(frame_str) < end_idx + 3:  # *后面至少要有2位
			return None, None
		
		# 提取数据部分（@ 和 * 之间）
		hex_data = frame_str[start_idx+1:end_idx]
		
		# 提取校验值（*后面的2位）
		recv_xor = frame_str[end_idx+1:end_idx+3]
		
		if len(hex_data) < 2:  # 至少2位命令ID
			return None, None
		
		if len(recv_xor) != 2:
			return None, None
		
		# 验证校验（计算 @ + 数据 + * 的XOR）
		calc_xor = calculate_xor('@' + hex_data + '*')
		
		if calc_xor != recv_xor.upper():
			log_message(f"校验错误: 收到={recv_xor}, 计算={calc_xor}")
			return None, None
		
		# 提取命令ID（前两位）
		cmd_id = hex_data[0:2].upper()
		
		return cmd_id, hex_data
		
	except Exception as e:
		log_message(f"协议解析错误: {e}")
		return None, None

def build_protocol_frame(resp_id: str, resp_hex_data: str) -> bytes:
	"""
	构建协议帧：@<RESP_ID><RESP_DATA>*<XOR>
	resp_id: 响应ID（十六进制字符串，如 "01"）
	resp_hex_data: 响应数据（十六进制字符串，如 "00 00 00 00"）
	返回: 完整的协议帧字节串
	"""
	# 去除空格，构建数据字符串
	hex_str = resp_id + resp_hex_data.replace(" ", "").upper()
	
	# 计算校验：@ + 数据 + *
	xor_val = calculate_xor('@' + hex_str + '*')
	
	# 构建完整帧：@<数据>*<校验>
	frame = '@' + hex_str + '*' + xor_val
	
	return frame.encode('ascii')

def process_received_data(data: bytes, send_func):
	"""
	处理接收到的数据，解析命令ID，并根据配置发送响应
	data: 接收到的字节数据
	send_func: 发送响应的函数，接受 bytes 参数
	"""
	if len(data) < 1:
		return
	
	# 解析协议帧
	cmd_id_str, hex_data = parse_protocol_frame(data)
	
	if cmd_id_str is None:
		log_message(f"无效的协议帧")
		return
	
	# 查找对应的命令配置
	if cmd_id_str not in cmd_settings:
		log_message(f"未知命令ID: {cmd_id_str}")
		return
	
	cfg = cmd_settings[cmd_id_str]
	
	# 更新总计数器（用于日志显示）
	if cmd_id_str not in cmd_counters:
		cmd_counters[cmd_id_str] = 0
	cmd_counters[cmd_id_str] += 1
	total_count = cmd_counters[cmd_id_str]
	
	# 更新连续计数器
	global last_cmd_id, consecutive_count
	if last_cmd_id == cmd_id_str:
		# 连续收到相同命令，计数+1
		consecutive_count += 1
	else:
		# 收到不同命令，重置连续计数
		consecutive_count = 1
		last_cmd_id = cmd_id_str
	
	start_after = cfg["start_after"].get()
	
	# 检查连续次数是否达到要求（需要连续收到 start_after+1 次才开始回复）
	if consecutive_count <= start_after:
		log_message(f"命令 {cfg['name']} (ID:{cmd_id_str}) 连续收到第 {consecutive_count} 次，需连续 {start_after+1} 次才回复")
		return
	
	# 达到连续次数要求，准备回复
	current_count = total_count
	
	# 获取延迟时间
	try:
		delay_ms = cfg["delay_ms"].get()
	except Exception as e:
		log_message(f"获取延迟时间失败: {e}")
		delay_ms = 0
	
	# 构建响应数据
	resp_id = cfg["resp_id"]
	resp_hex = cfg["resp_hex"].get().strip()
	
	if not resp_hex:
		log_message(f"命令 {cfg['name']} 无响应数据配置")
		return
	
	# 应用延迟并发送响应
	def send_delayed():
		try:
			# 先延迟
			if delay_ms > 0:
				log_message(f"延迟 {delay_ms}ms...")
				time.sleep(delay_ms / 1000.0)
			
			# 构建协议帧
			full_response = build_protocol_frame(resp_id, resp_hex)
			
			# 发送
			send_func(full_response)
			
			# 发送完成后再打印日志（这样时间戳就是真正发送的时间）
			ascii_str = full_response.decode('ascii')
			hex_str = " ".join(f"{b:02X}" for b in full_response)
			log_message(f"TX [ASCII]: {ascii_str} (命令:{cfg['name']}, 第{current_count}次)")
			log_message(f"TX [HEX]: {hex_str}")
		except Exception as e:
			log_message(f"发送响应失败: {e}")
	
	# 在单独线程中执行（避免阻塞读取线程）
	threading.Thread(target=send_delayed, daemon=True).start()

class TcpManager:
	def __init__(self):
		self.listen_sock = None
		self.conn = None
		self.accept_thread = None
		self.stop_event = threading.Event()

	def is_connected(self):
		# 表示服务器已启动
		return self.listen_sock is not None

	def start_server(self, host: str, port: int):
		try:
			self.listen_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
			self.listen_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
			self.listen_sock.bind((host, port))
			self.listen_sock.listen(1)
			self.listen_sock.settimeout(0.5)
			self.stop_event.clear()
			self.accept_thread = threading.Thread(target=self._accept_loop, daemon=True)
			self.accept_thread.start()
			log_message(f"TCP服务已启动: {host}:{port}，等待客户端连接…")
			return True
		except Exception as e:
			log_message(f"TCP服务启动失败: {e}")
			self.listen_sock = None
			return False

	def stop_server(self):
		try:
			self.stop_event.set()
			if self.accept_thread and self.accept_thread.is_alive():
				self.accept_thread.join(timeout=1.0)
		except Exception:
			pass
		try:
			if self.conn:
				try:
					self.conn.shutdown(socket.SHUT_RDWR)
				except Exception:
					pass
				self.conn.close()
		except Exception:
			pass
		try:
			if self.listen_sock:
				self.listen_sock.close()
		except Exception:
			pass
		self.conn = None
		self.listen_sock = None
		self.accept_thread = None
	
	def send(self, conn, data: bytes):
		"""发送数据到TCP客户端"""
		try:
			if conn:
				conn.sendall(data)
		except Exception as e:
			log_message(f"TCP发送错误: {e}")

	def _accept_loop(self):
		while not self.stop_event.is_set():
			try:
				try:
					conn, addr = self.listen_sock.accept()
				except socket.timeout:
					continue
				self.conn = conn
				self.conn.settimeout(0.2)
				log_message(f"TCP客户端已连接: {addr}")
				# 读循环（直到客户端断开或服务停止）
				while not self.stop_event.is_set():
					try:
						data = self.conn.recv(1024)
						if not data:
							# recv返回空字节串表示客户端已关闭连接
							log_message("TCP客户端断开连接")
							break
						
						# 显示原始十六进制和ASCII解码
						hex_str = " ".join(f"{b:02X}" for b in data)
						ascii_str = data.decode('ascii', errors='ignore')
						log_message(f"TCP RX [HEX]: {hex_str}")
						log_message(f"TCP RX [ASCII]: {ascii_str}")
						# 处理接收到的数据并发送响应
						process_received_data(data, lambda d: self.send(self.conn, d))
					except socket.timeout:
						# 超时是正常的，继续等待数据
						continue
					except Exception as e:
						log_message(f"TCP读取错误: {e}")
						break
				try:
					self.conn.close()
				except Exception:
					pass
				self.conn = None
				log_message("TCP客户端已断开，继续等待新客户端…")
			except Exception as e:
				log_message(f"TCP接受错误: {e}")
				time.sleep(0.2)

tcp_mgr = TcpManager()

connected_mode = None  # None / 'serial' / 'tcp'

def update_conn_mode(event=None):
	mode = mode_combo.get()
	if connected_mode:
		return
	# 先隐藏两个子框，再显示对应模式
	try:
		serial_frame.pack_forget()
	except Exception:
		pass
	try:
		tcp_frame.pack_forget()
	except Exception:
		pass
	if mode == "串口":
		serial_frame.pack(side="left")
	else:
		tcp_frame.pack(side="left")

def scan_ports():
	ports = []
	try:
		if list_ports is not None:
			ports = [p.device for p in list_ports.comports()]
	except Exception as e:
		log_message(f"扫描串口失败: {e}")
	port_combo["values"] = ports
	if ports:
		port_combo.set(ports[0])

def on_connect_click():
	global connected_mode
	# 如果已连接则断开当前模式
	if connected_mode == 'serial' and serial_mgr.is_connected():
		serial_mgr.disconnect()
		connected_mode = None
		connect_btn.configure(text="连接")
		port_combo.configure(state="normal")
		baud_entry.configure(state="normal")
		mode_combo.configure(state="normal")
		log_message("已断开串口连接")
		update_conn_mode()
		return
	if connected_mode == 'tcp' and tcp_mgr.is_connected():
		tcp_mgr.stop_server()
		connected_mode = None
		connect_btn.configure(text="连接")
		ip_entry.configure(state="normal")
		tcp_port_entry.configure(state="normal")
		mode_combo.configure(state="normal")
		log_message("已断开TCP连接")
		update_conn_mode()
		return

	# 开始连接
	mode = mode_combo.get()
	if mode == "串口":
		port = port_combo.get().strip()
		try:
			baud = int(baud_entry.get())
		except Exception:
			baud = 9600
		if not port:
			log_message("请先选择或输入串口端口")
			return
		ok = serial_mgr.connect(port, baud)
		if ok:
			connected_mode = 'serial'
			connect_btn.configure(text="断开")
			port_combo.configure(state="disabled")
			baud_entry.configure(state="disabled")
			mode_combo.configure(state="disabled")
			log_message(f"串口已连接: {port} @ {baud}")
	else:
		host = ip_entry.get().strip() or "127.0.0.1"
		try:
			tcp_port = int(tcp_port_entry.get())
		except Exception:
			tcp_port = 9000
		ok = tcp_mgr.start_server(host, tcp_port)
		if ok:
			connected_mode = 'tcp'
			connect_btn.configure(text="停止")
			ip_entry.configure(state="disabled")
			tcp_port_entry.configure(state="disabled")
			mode_combo.configure(state="disabled")
			log_message(f"TCP服务运行中: {host}:{tcp_port}")

def poll_logs():
	try:
		while True:
			ts, msg = log_queue.get_nowait()
			append_log(ts, msg)
	except queue.Empty:
		pass
	root.after(100, poll_logs)

def on_close():
	try:
		serial_mgr.disconnect()
		tcp_mgr.stop_server()
	except Exception:
		pass
	root.destroy()

# 绑定按钮事件
refresh_btn.configure(command=scan_ports)
connect_btn.configure(command=on_connect_click)
mode_combo.bind("<<ComboboxSelected>>", update_conn_mode)
browse_btn.configure(command=browse_config_file)
load_btn.configure(command=load_config_file)

# 初始扫描一次
scan_ports()
update_conn_mode()

# 启动日志轮询
root.after(100, poll_logs)

root.protocol("WM_DELETE_WINDOW", on_close)
root.mainloop()