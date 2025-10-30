import socket
import time
import json
import psutil
import cpuinfo
import GPUtil
import threading
import traceback
import multiprocessing
from datetime import datetime
import sys
import os
from PIL import Image, ImageDraw

# 尝试导入系统托盘相关库
try:
    import pystray
    from pystray import MenuItem as item
    TRAY_AVAILABLE = True
except ImportError:
    TRAY_AVAILABLE = False
    # 在.pyw运行时，无法显示控制台输出，所以需要将错误信息记录到文件
    with open(os.path.join(os.path.dirname(__file__), 'error.log'), 'a') as f:
        f.write("系统托盘功能不可用，请安装pystray和Pillow库: pip install pystray Pillow\n")

def resource_path(relative_path):
    """获取资源的绝对路径。用于PyInstaller打包后定位资源文件"""
    try:
        # PyInstaller创建临时文件夹，将路径存储在_MEIPASS中
        base_path = sys._MEIPASS
    except Exception:
        base_path = os.path.abspath(".")
    return os.path.join(base_path, relative_path)

def create_default_icon():
    """创建默认图标"""
    # 创建一个简单的图标
    image = Image.new('RGB', (64, 64), color='blue')
    dc = ImageDraw.Draw(image)
    dc.text((10, 10), "SM", fill='white')  # SM for System Monitor
    return image
    
class SystemMonitorServer:
    def __init__(self, host='0.0.0.0', port=23333, update_interval=1.0):
        self.host = host
        self.port = port
        self.update_interval = update_interval
        self.server_socket = None
        self.running = False
        
        # 系统信息缓存
        self.system_info = {}
        self.info_lock = threading.Lock()
        self.last_update_time = 0
        
        # 网络统计
        self.net_stats = {
            'upload_speed': 0,
            'download_speed': 0,
            'prev_upload': 0,
            'prev_download': 0,
            'last_update': time.time()
        }
        
        # 缓存不常变化的信息
        self.cpu_model = self._get_cpu_model()
        self.gpu_static_info = self._get_gpu_static_info()
        
        # 系统托盘图标
        self.tray_icon = None
        self.local_ip = self.get_local_ip()
        
        # 初始化网络速度监控
        self.init_network_stats()
        
        # 创建日志文件
        self.log_file = os.path.join(os.path.dirname(__file__), 'system_monitor.log')
        
    def log_message(self, message):
        """记录日志消息"""
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        with open(self.log_file, 'a') as f:
            f.write(f"[{timestamp}] {message}\n")
    
    def get_local_ip(self):
        """获取局域网IP地址"""
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect(("8.8.8.8", 80))
            ip = s.getsockname()[0]
            s.close()
            return ip
        except Exception as e:
            error_msg = f"获取本地IP地址失败: {e}"
            self.log_message(error_msg)
            return "无法获取IP"
    
    def create_tray_icon(self):
        """创建系统托盘图标"""
        if not TRAY_AVAILABLE:
            return
            
        try:
            # 尝试加载图标文件
            icon_path = resource_path("icon.ico")
            if os.path.exists(icon_path):
                icon = Image.open(icon_path)
            else:
                # 如果图标文件不存在，创建默认图标
                self.log_message("图标文件未找到，使用默认图标")
                icon = create_default_icon()
            
            # 创建菜单
            menu = (
                item(f"IP: {self.local_ip}", lambda: None, enabled=False),
                item(f"端口: {self.port}", lambda: None, enabled=False),
                item("退出", self.quit_application)
            )
            
            # 创建系统托盘图标
            self.tray_icon = pystray.Icon("system_monitor", icon, "系统监控服务器", menu)
        except Exception as e:
            self.log_message(f"创建系统托盘图标失败: {e}")
            self.tray_icon = None
    
    def run_tray_icon(self):
        """运行系统托盘图标"""
        if self.tray_icon:
            try:
                self.tray_icon.run()
            except Exception as e:
                self.log_message(f"运行系统托盘图标失败: {e}")
    
    def quit_application(self, icon=None, item=None):
        """退出应用程序"""
        self.log_message("正在退出应用程序...")
        self.running = False
        if self.server_socket:
            try:
                self.server_socket.close()
            except:
                pass
        if self.tray_icon:
            try:
                self.tray_icon.stop()
            except:
                pass
        os._exit(0)
    
    def _get_cpu_model(self):
        """获取CPU型号（不常变化的信息）"""
        try:
            info = cpuinfo.get_cpu_info()
            return info.get('brand_raw', 'Unknown CPU')
        except:
            return 'Unknown CPU'
    
    def _get_gpu_static_info(self):
        """获取GPU静态信息（不常变化的信息）"""
        try:
            gpus = GPUtil.getGPUs()
            return [{'id': gpu.id, 'model': gpu.name, 'memory_total': gpu.memoryTotal} for gpu in gpus]
        except:
            return [{'model': 'No GPU detected', 'memory_total': 0}]
    
    def init_network_stats(self):
        """初始化网络统计"""
        try:
            net_io = psutil.net_io_counters()
            self.net_stats['prev_upload'] = net_io.bytes_sent
            self.net_stats['prev_download'] = net_io.bytes_recv
            self.net_stats['last_update'] = time.time()
        except Exception as e:
            self.log_message(f"Error initializing network stats: {e}")
    
    def update_network_speed(self):
        """更新网络速度 - 优化版本"""
        try:
            current_time = time.time()
            time_diff = current_time - self.net_stats['last_update']
            
            if time_diff < 0.3:  # 至少0.3秒更新一次
                return
            
            net_io = psutil.net_io_counters()
            current_download = net_io.bytes_recv
            current_upload = net_io.bytes_sent
            
            # 计算速度 (bytes per second)
            download_speed = (current_download - self.net_stats['prev_download']) / time_diff
            upload_speed = (current_upload - self.net_stats['prev_upload']) / time_diff
            
            # 转换为MB/s
            download_speed_mb = download_speed / (1024 * 1024)
            upload_speed_mb = upload_speed / (1024 * 1024)
            
            # 更新状态
            self.net_stats['prev_download'] = current_download
            self.net_stats['prev_upload'] = current_upload
            self.net_stats['last_update'] = current_time
            self.net_stats['download_speed'] = download_speed_mb
            self.net_stats['upload_speed'] = upload_speed_mb
        except Exception as e:
            self.log_message(f"Error updating network speed: {e}")
    
    def get_cpu_info(self):
        """获取CPU信息 - 优化版本"""
        try:
            # 获取CPU使用率 - 使用更短的间隔
            cpu_percent = psutil.cpu_percent(interval=0.1)
            
            # 获取每个核心的使用率
            cpu_percent_per_core = psutil.cpu_percent(interval=0.1, percpu=True)
            
            # 获取CPU频率
            cpu_freq = psutil.cpu_freq()
            current_freq = cpu_freq.current if cpu_freq else None
            
            # 获取CPU温度 - 减少检查频率
            cpu_temp = None
            if time.time() - self.last_update_time > 5:  # 每5秒检查一次温度
                if hasattr(psutil, "sensors_temperatures"):
                    temps = psutil.sensors_temperatures()
                    if 'coretemp' in temps:
                        cpu_temp = max([temp.current for temp in temps['coretemp'] if hasattr(temp, 'current')])
                    elif temps:
                        for key, values in temps.items():
                            if values and hasattr(values[0], 'current'):
                                cpu_temp = values[0].current
                                break
            
            return {
                'model': self.cpu_model,  # 使用缓存的CPU型号
                'usage_percent': cpu_percent,
                'usage_per_core': cpu_percent_per_core,
                'frequency': {
                    'current': current_freq,
                },
                'temperature': cpu_temp
            }
        except Exception as e:
            self.log_message(f"Error getting CPU info: {e}")
            return {
                'model': self.cpu_model,
                'usage_percent': 0,
                'usage_per_core': [],
                'frequency': {
                    'current': None,
                },
                'temperature': None
            }
    
    def get_gpu_info(self):
        """获取GPU信息 - 优化版本"""
        try:
            gpu_info_list = []
            
            # 使用缓存的静态信息，只获取动态信息
            for i, static_info in enumerate(self.gpu_static_info):
                try:
                    # 只获取当前GPU的动态信息
                    gpu = GPUtil.getGPUs()[i] if i < len(GPUtil.getGPUs()) else None
                    if gpu:
                        gpu_info_list.append({
                            'id': static_info['id'],
                            'model': static_info['model'],
                            'usage_percent': gpu.load * 100,
                            'temperature': gpu.temperature,
                            'memory_total': static_info['memory_total'],
                            'memory_used': gpu.memoryUsed,
                            'memory_percent': gpu.memoryUtil * 100
                        })
                    else:
                        gpu_info_list.append({
                            'model': static_info['model'],
                            'usage_percent': 0,
                            'temperature': None,
                            'memory_total': static_info['memory_total'],
                            'memory_used': 0,
                            'memory_percent': 0
                        })
                except:
                    # 如果获取单个GPU信息失败，使用静态信息
                    gpu_info_list.append({
                        'model': static_info['model'],
                        'usage_percent': 0,
                        'temperature': None,
                        'memory_total': static_info['memory_total'],
                        'memory_used': 0,
                        'memory_percent': 0
                    })
            
            return gpu_info_list
        except Exception as e:
            self.log_message(f"Error getting GPU info: {e}")
            return [{
                'model': 'Error',
                'usage_percent': 0,
                'temperature': None,
                'memory_total': 0,
                'memory_used': 0,
                'memory_percent': 0
            }]
    
    def get_ram_info(self):
        """获取RAM信息"""
        try:
            virtual_mem = psutil.virtual_memory()
            return {
                'total': virtual_mem.total / (1024 ** 3),  # 转换为GB
                'available': virtual_mem.available / (1024 ** 3),  # 转换为GB
                'used': virtual_mem.used / (1024 ** 3),    # 转换为GB
                'usage_percent': virtual_mem.percent,
            }
        except Exception as e:
            self.log_message(f"Error getting RAM info: {e}")
            return {
                'total': 0,
                'available': 0,
                'used': 0,
                'usage_percent': 0,
            }
    
    def get_disk_info(self):
        """获取硬盘信息 - 优化版本，减少更新频率"""
        try:
            # 每10秒更新一次磁盘信息
            if time.time() - self.last_update_time > 10:
                disks = []
                for partition in psutil.disk_partitions():
                    if 'cdrom' in partition.opts or partition.fstype == '':
                        continue
                    try:
                        usage = psutil.disk_usage(partition.mountpoint)
                        disks.append({
                            'device': partition.device,
                            'mountpoint': partition.mountpoint,
                            'fstype': partition.fstype,
                            'total': usage.total / (1024 ** 3),  # 转换为GB
                            'used': usage.used / (1024 ** 3),    # 转换为GB
                            'free': usage.free / (1024 ** 3),    # 转换为GB
                            'usage_percent': usage.percent
                        })
                    except (PermissionError, OSError) as e:
                        # 可能无法访问某些磁盘
                        continue
                return disks
            else:
                # 返回上次的磁盘信息
                with self.info_lock:
                    return self.system_info.get('disks', [])
        except Exception as e:
            self.log_message(f"Error getting disk info: {e}")
            return []
    
    def update_system_info(self):
        """更新系统信息"""
        while self.running:
            try:
                # 更新网络速度
                self.update_network_speed()
                
                # 获取所有系统信息
                system_info = {
                    'timestamp': datetime.now().isoformat(),
                    'cpu': self.get_cpu_info(),
                    'gpu': self.get_gpu_info(),
                    'ram': self.get_ram_info(),
                    'disks': self.get_disk_info(),
                    'network': {
                        'upload_speed': self.net_stats['upload_speed'],
                        'download_speed': self.net_stats['download_speed']
                    }
                }
                
                # 使用锁更新共享数据
                with self.info_lock:
                    self.system_info = system_info
                
                self.last_update_time = time.time()
                
                # 等待下一次更新
                time.sleep(self.update_interval)
            except Exception as e:
                self.log_message(f"Error updating system info: {e}")
                time.sleep(self.update_interval)
    
    def handle_client(self, client_socket, addr):
        """处理客户端连接 - 优化版本"""
        try:
            # 使用锁获取系统信息
            with self.info_lock:
                system_info = self.system_info.copy()
            
            # 使用更紧凑的JSON格式
            json_data = json.dumps(system_info, separators=(',', ':'))
            
            # 发送数据给客户端
            client_socket.sendall(json_data.encode('utf-8'))
            
        except Exception as e:
            self.log_message(f"Error handling client {addr}: {e}")
        finally:
            try:
                client_socket.close()
            except:
                pass
    
    def start_server(self):
        """启动服务器"""
        try:
            self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.server_socket.bind((self.host, self.port))
            self.server_socket.listen(5)
            
            self.log_message(f"System monitor server started on {self.host}:{self.port}")
            self.log_message(f"Local IP: {self.local_ip}")
            self.log_message(f"Port: {self.port}")
            
            # 启动系统信息更新线程
            self.running = True
            update_thread = threading.Thread(target=self.update_system_info)
            update_thread.daemon = True
            update_thread.start()
            
            while self.running:
                try:
                    client_socket, addr = self.server_socket.accept()
                    
                    # 在新线程中处理客户端
                    client_thread = threading.Thread(
                        target=self.handle_client,
                        args=(client_socket, addr)
                    )
                    client_thread.daemon = True
                    client_thread.start()
                    
                except socket.timeout:
                    continue
                except OSError as e:
                    if self.running:
                        self.log_message(f"Error accepting connection: {e}")
                    break
                except Exception as e:
                    self.log_message(f"Unexpected error accepting connection: {e}")
                    if self.running:
                        time.sleep(0.1)  # 短暂等待后继续
                    
        except KeyboardInterrupt:
            self.log_message("Server shutting down due to KeyboardInterrupt...")
        except Exception as e:
            self.log_message(f"Server error: {e}")
        finally:
            self.running = False
            if self.server_socket:
                try:
                    self.server_socket.close()
                except:
                    pass
            self.log_message("Server stopped")

if __name__ == "__main__":
    multiprocessing.freeze_support()
    server = SystemMonitorServer()
    
    # 创建系统托盘图标
    server.create_tray_icon()
    
    # 在单独线程中启动服务器
    server_thread = threading.Thread(target=server.start_server)
    server_thread.daemon = False  # 设置为非守护线程，确保主线程退出时服务器线程不会立即终止
    server_thread.start()
    
    # 运行系统托盘图标（在主线程中）
    if TRAY_AVAILABLE and server.tray_icon:
        server.run_tray_icon()
    else:
        # 如果没有系统托盘支持，等待服务器线程结束
        try:
            server_thread.join()
        except KeyboardInterrupt:
            server.running = False
            server.log_message("程序已退出")