#include "socketsystem.h"

namespace LSW {
    namespace v5 {
        namespace Sockets {

            bool connection_core::initialize(const char* ip_str, const int port, const int isthis_ipv6)
            {
                if (init) return true;

                if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return [&]{failure = true; return false;}();

                struct addrinfo hints;

                char port_str[16];
                sprintf_s(port_str, "%d", port);

                SecureZeroMemory(&hints, sizeof(hints));
                if (isthis_ipv6 >= 0) hints.ai_family = isthis_ipv6 ? AF_INET6 : AF_INET;
                else hints.ai_family = AF_UNSPEC;
                hints.ai_socktype = SOCK_STREAM;
                hints.ai_protocol = IPPROTO_TCP;
                if (isthis_ipv6 >= 0) hints.ai_flags = AI_PASSIVE;

                // Resolve the server address and port
                if (getaddrinfo(ip_str, port_str, &hints, &result) != 0) return [&]{failure = true; return false;}();

                init = true;
                return true;
            }

            bool connection_core::as_client(SOCKET& ConnectSocket)
            {
                if (failure || !init) return false;

                struct addrinfo* ptr = NULL;

                // Attempt to connect to an address until one succeeds
                for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

                    // Create a SOCKET for connecting to server
                    ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
                        ptr->ai_protocol);
                    if (ConnectSocket == INVALID_SOCKET) return false;

                    // Connect to server.
                    if (connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR) {
                        closesocket(ConnectSocket);
                        ConnectSocket = INVALID_SOCKET;
                        continue;
                    }
                    break;
                }

                freeaddrinfo(result);

                if (ConnectSocket == INVALID_SOCKET) return false;
                // connected
                return true;
            }

            bool connection_core::as_host(SOCKET& ListenSocket)
            {
                if (failure || !init) return false;

                ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
                if (ListenSocket == INVALID_SOCKET) return false;

                // Setup the TCP listening socket
                if (bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
                    freeaddrinfo(result);
                    closesocket(ListenSocket);
                    return false;
                }

                freeaddrinfo(result);
                // ready to listen
                return true;
            }



            void con_client::start_threads()
            {
                if (!thr_recv) thr_recv = Tools::new_guaranteed<std::thread>([&]() {__thr_recv(); });
                if (!thr_send) thr_send = Tools::new_guaranteed<std::thread>([&]() {__thr_send(); });
                if (!thr_mont) thr_mont = Tools::new_guaranteed<std::thread>([&]() {__thr_mont(); }); // kinda light task, shall see later
            }

            void con_client::__set_kill()
            {
                kill_threads = true;
                latest_update = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
                wait_recv.signal_all();
                send_wait.signal_all();
            }

            /*void con_client::__task_recv()
            {
                task_recv++;
            }*/

            /*void con_client::__task_send()
            {
                task_send++;
            }*/

            void con_client::__ploss_recv()
            {
                package_loss_recv++;
            }

            void con_client::__ploss_send()
            {
                package_loss_send++;
            }

            int con_client::__send_small(__internal_package& small_p)
            {
                if (nt_prunt) nt_prunt("s " + std::to_string(small_p.data_len) + "B");
                auto ret = ::send(Connected, (const char*)(&(small_p)), sizeof(__internal_package), 0);
                if (ret) { task_send += ret; task_totl += ret; }
                return ret > 1 ? ret == sizeof(__internal_package) : ret;
            }

            int con_client::__recv_small(__internal_package& small_p)
            {
                auto ret = ::recv(Connected, (char*)&small_p, sizeof(__internal_package), 0);
                if (nt_prunt) nt_prunt("r " + std::to_string(small_p.data_len) + "B");
                if (ret) { task_recv += ret; task_totl += ret; }
                return ret > 1 ? ret == sizeof(__internal_package) : ret;
            }

            void con_client::__thr_send()
            {
                auto handle_send = [&](__internal_package& small_p)->bool {
                    int result = 0;
                    do {
                        result = __send_small(small_p);
                        if (result == 0) {
                                __ploss_send();
                        }
                        else if (result < 0) {
                            if (bw_prunt) bw_prunt("Connection failed.");
                            __set_kill();
                            return false;
                        }
                    } while (result <= 0);
                    return true;
                };

                while (!kill_threads) {
                    if (sending.size() > 0) {

                        send_wait.wait_signal();

                        sending_m.lock();

                        for (auto& pkg : sending) {
                            __internal_package small_p;

                            if (!pkg) {
                                if (nt_prunt) nt_prunt("[PKG#" + std::to_string(total_sockets_sr) + "] NULL PACKAGE SKIP!");
                                continue;
                            }

                            size_t remaining = pkg->variable_data.size();
                            char* pos = pkg->variable_data.data();

                            small_p.data_type = pkg->data_type; // still the same till the end

                            if (remaining == 0) {
                                small_p.combine_with_n_more = 0;
                                small_p.data_len = 0;
                                if (!handle_send(small_p)) continue;
                                if (nt_prunt) nt_prunt("[PKG#" + std::to_string(total_sockets_sr) + "] SENT N-PACKAGE #" + std::to_string(small_p.data_type) + "!");
                                total_sockets_sr++;
                            }
                            else {
                                while (remaining > 0) {
                                    small_p.combine_with_n_more = static_cast<int>((remaining - 1) / default_package_size);

                                    size_t expected = default_package_size;
                                    if (remaining < expected) { expected = remaining; remaining = 0; }
                                    else remaining -= default_package_size;

                                    memcpy_s(small_p.data, default_package_size, pos, expected);
                                    pos += expected;
                                    small_p.data_len = static_cast<int>(expected);

                                    if (!handle_send(small_p)) continue;                                    

                                    //__task_send();
                                }
                                if (nt_prunt) nt_prunt("[PKG#" + std::to_string(total_sockets_sr) + "] SENT PACKAGE #" + std::to_string(small_p.data_type) + "!");
                                total_sockets_sr++;
                            }

                            delete pkg;
                            pkg = nullptr;
                            sending.erase(sending.begin());
                        }

                        sending_m.unlock();
                    }
                }
            }

            void con_client::__thr_recv()
            {
                final_package pkg;
                auto handle_recv = [&](__internal_package& small_p)->bool {
                    int result = 0;
                    do {
                        result = __recv_small(small_p);
                        if (result == 0) {
                            __ploss_recv();
                        }
                        else if (result < 0) {
                            if (bw_prunt) bw_prunt("Connection failed.");
                            __set_kill();
                            return false;
                        }
                    } while (result <= 0);
                    return true;
                };
                auto push = [&](final_package& pkg) {received_m.lock(); received.push_back(Tools::new_guaranteed<final_package>(pkg.variable_data, pkg.data_type)); auto siz = received.size(); received_m.unlock(); wait_recv.signal_all(); pkg.data_type = 0; pkg.variable_data.clear(); return siz; };

                while (!kill_threads) {

                    __internal_package small_p;

                    if (!handle_recv(small_p)) continue;

                    if (small_p.data_len == 0) {
                        auto cpyy = pkg.data_type;
                        auto len = push(pkg);
                        if (nt_prunt) nt_prunt("[PKG#" + std::to_string(total_sockets_sr) + "] RECEIVED N-PACKAGE #" + std::to_string(cpyy) + " [" + std::to_string(len) + " PKGS IN BUFFER]");
                        total_sockets_sr++;
                        continue;
                    }               

                    if (!pkg.variable_data.size()) pkg.data_type = small_p.data_type;

                    pkg.variable_data.cat(static_cast<char*>(small_p.data), small_p.data_len);

                    if (small_p.combine_with_n_more == 0) {
                        auto cpyy = pkg.data_type;
                        auto len = push(pkg);
                        if (nt_prunt) nt_prunt("[PKG#" + std::to_string(total_sockets_sr) + "] RECEIVED PACKAGE #" + std::to_string(cpyy) + " [" + std::to_string(len) + " PKGS IN BUFFER]");
                        total_sockets_sr++;
                    }

                    //__task_recv();
                }
            }

            void con_client::__thr_mont()
            {
                std::chrono::milliseconds upd = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

                size_t latest_end_task_recv = end_task_recv + 1;
                size_t latest_end_task_send = end_task_send + 1;
                size_t latest_task_totl = task_totl;

                while (!kill_threads) {
                    auto _t = (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()) - upd);
                    if (_t.count() < 1000) std::this_thread::sleep_for(std::chrono::milliseconds(1000) - _t);
                    upd = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

                    end_task_recv = task_recv;
                    task_recv = 0;
                    end_task_send = task_send;
                    task_send = 0;

                    if (bw_prunt && (end_task_recv != latest_end_task_recv || end_task_send != latest_end_task_send) || (latest_task_totl != task_totl)) {
                        latest_end_task_recv = end_task_recv;
                        latest_end_task_send = end_task_send;
                        latest_task_totl = task_totl;

                        /*"▲" " ▼"*/

                        bw_prunt(
                            std::string("^") + Tools::byteAutoString(end_task_send) + "B/s" +
                            (end_package_loss_send > 0 ? (" PL:" + std::to_string(end_package_loss_send) + "/s") : "") +
                            " v" + Tools::byteAutoString(end_task_recv) + "B/s" +
                            (end_package_loss_recv > 0 ? (" PL:" + std::to_string(end_package_loss_recv) + "/s") : "") +
                            " T" + Tools::byteAutoString(task_totl) + "B");
                    }
                }
            }

            con_client::con_client(SOCKET already_connected) : core()
            {
                if (already_connected != INVALID_SOCKET) {
                    Connected = already_connected;
                    start_threads();
                    latest_update = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
                }
            }

            con_client::~con_client()
            {
                kill();
            }

            bool con_client::connect(const char* a, const int b)
            {
                if (!core.initialize(a, b, -1)) return false;
                if (!core.as_client(Connected)) return false;
                start_threads();
                latest_update = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
                //start_internally_as_client();
                return true;
            }

            void con_client::kill()
            {
                __set_kill();
                if (Connected != INVALID_SOCKET) closesocket(Connected);
                Connected = INVALID_SOCKET;
                if (thr_mont) {
                    thr_mont->join();
                    delete thr_mont;
                    thr_mont = nullptr;
                }
                if (thr_recv) {
                    thr_recv->join();
                    delete thr_recv;
                    thr_recv = nullptr;
                }
                if (thr_send) {
                    thr_send->join();
                    delete thr_send;
                    thr_send = nullptr;
                }
                for (auto& i : received) if (i) { delete i; i = nullptr; }
                for (auto& i : sending) if (i) { delete i; i = nullptr; }
                received.clear();
                sending.clear();
            }

            bool con_client::isConnected()
            {
                return !kill_threads;
            }

            size_t con_client::hasPackage()
            {
                return received.size();
            }

            size_t con_client::hasSending()
            {
                return sending.size();
            }

            void con_client::send(final_package&& w)
            {
                sending_m.lock();
                sending.push_back(Tools::new_guaranteed<final_package>(w.variable_data, w.data_type));
                sending_m.unlock();
                send_wait.signal_all();
            }

            bool con_client::recv(final_package& end, bool wait)
            {
                if (received.size() == 0 && !wait) return false;
                while (received.size() == 0 && wait) { wait_recv.wait_signal(connection_timeout_speed); }

                received_m.lock();
                auto& i = received[0];
                end.data_type = i->data_type;
                end.variable_data = i->variable_data;
                delete i;
                i = nullptr;
                received.erase(received.begin());
                received_m.unlock();
                return true;
            }

            bool con_client::hadEventRightNow()
            {
                return ((std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()) - latest_update).count() < connection_timeout_speed);
            }

            void con_client::hookPrintBandwidth(std::function<void(const std::string)> f)
            {
                bw_prunt = f;
            }

            void con_client::hookPrintEvent(std::function<void(const std::string)> f)
            {
                nt_prunt = f;
            }

            void con_host::auto_accept()
            {
                while (still_running) {
                    if (listen(Listening, SOMAXCONN) == SOCKET_ERROR) continue;

                    // Accept a client socket
                    SOCKET ClientSocket = accept(Listening, NULL, NULL);
                    if (ClientSocket == INVALID_SOCKET) continue;

                    con_client* dis = Tools::new_guaranteed<con_client>(ClientSocket);
                    if (connections.size() >= max_connections_allowed) {
                        dis->kill();
                        delete dis;
                        continue;
                    }

                    //dis->start_internally_as_host(); // cause sended last time, so should receive so there's no error

                    connections_m.lock();
                    connections.push_back(dis);
                    connections_m.unlock();

                    wait_new_connection.signal_all();

                    if (bw_prunt) bw_prunt("Someone has connected!");
                    //printf_s("\nSomeone has connected!");

                }
                closesocket(Listening);
                Listening = INVALID_SOCKET;
            }

            void con_host::auto_cleanup()
            {
                while (still_running) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(connection_timeout_speed));

                    connections_m.lock();

                    for (size_t p = 0; p < connections.size(); p++)
                    {
                        auto& i = connections[p];
                        if (!i->isConnected() && !i->hadEventRightNow()) { // at least stay dead for a while
                            i->kill();
                            delete i;
                            connections.erase(connections.begin() + p--);
                            wait_new_connection.signal_all();
                            if (bw_prunt) bw_prunt("Someone has disconnected.");
                            //printf_s("\nSomeone has disconnected!");
                        }
                    }

                    connections_m.unlock();
                }
            }

            void con_host::_initialize(const char* a, const int b, const bool c)
            {
                core.initialize(a, b, c);
                core.as_host(Listening);
                still_running = true;
                listener = Tools::new_guaranteed<std::thread>([&]() {auto_accept(); });
                autodisconnect = Tools::new_guaranteed<std::thread>([&]() {auto_cleanup(); });
            }

            con_host::con_host(const int port, const bool ipv6)
            {
                _initialize(nullptr, port, ipv6);
            }

            con_host::con_host(const bool ipv6)
            {
                _initialize(nullptr, default_port, ipv6);
            }

            con_host::~con_host()
            {
                setMaxConnections(0);
                still_running = false;
                for (auto& i : *this) i->kill();
                con_client oopsie; // unlock auto_accept (trigger a loop)
                oopsie.connect();
                listener->join();
                autodisconnect->join();
                delete listener;
                delete autodisconnect;
                listener = nullptr;
                autodisconnect = nullptr;
                connections.clear();
            }

            void con_host::lock()
            {
                connections_m.lock();
            }

            void con_host::unlock()
            {
                connections_m.unlock();
            }

            std::vector<con_client*>::iterator con_host::begin()
            {
                return connections.begin();
            }

            std::vector<con_client*>::iterator con_host::end()
            {
                return connections.end();
            }

            size_t con_host::size()
            {
                return connections.size();
            }
            void con_host::setMaxConnections(const size_t v)
            {
                max_connections_allowed = v;
            }
            con_client* con_host::waitNewConnection(const size_t timeout)
            {
                auto gTime = [] {return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); };
                auto tnow = gTime();

                size_t now = size();
                if (connections.size() > 0) {
                    if (connections.back()->hadEventRightNow()) return connections.back();
                }
                while (connections.size() <= now) {
                    if (timeout && static_cast<size_t>(gTime() - tnow) > timeout) return nullptr;
                    wait_new_connection.wait_signal(200);
                    if (now > size()) now--;
                }
                return connections.back(); // latest push_back
            }
            void con_host::hookPrintBandwidth(std::function<void(const std::string)> f)
            {
                bw_prunt = f;
            }
        }
    }
}