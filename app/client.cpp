#include <iostream>
#include "ikcp.h"
#include "uv.h"
#include "common.h"

namespace znet
{
    struct zclient
    {
        ikcpcb *kcp;
        uv_loop_t *loop;
        uv_udp_t *handle;
        uv_timer_t *kcp_timer;
        sockaddr_in remote_addr;
        uv_udp_send_t *send_req;
        uv_pipe_t *stdin_pipe; // for test
    };

    zclient *client;

    void on_send(uv_udp_send_t *req, int status);

    int output(const char *buf, int len, ikcpcb *kcp, void *user)
    {
        if (client->send_req == nullptr)
        {
            client->send_req = (uv_udp_send_t *)malloc(sizeof(uv_udp_send_t));
        }

        uv_buf_t uv_buf = uv_buf_init((char *)malloc(len), len);
        memcpy(uv_buf.base, buf, len);

        return uv_udp_send(client->send_req, client->handle, &uv_buf, 1, nullptr, on_send);
    }

    void on_close(uv_handle_t *handle)
    {
        if (client != nullptr)
        {
            free(client);
            client = nullptr;
        }
    }

    void close()
    {
        if (client == nullptr)
        {
            return;
        }
        uv_timer_stop(client->kcp_timer);
        uv_close((uv_handle_t *)client->kcp_timer, nullptr);
        uv_close((uv_handle_t *)client->handle, on_close);
    }

    void on_recv(uv_udp_t *handle, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr, unsigned flags)
    {
        if (nread < 0)
        {
            if (nread == UV_EOF)
            {
                std::cerr << "EOF!" << std::endl;
            }
            std::cerr << "Read failed! Close Connection!" << std::endl;
            close();
            return;
        }
        std::cout << "Received data: " << buf->base << "\t Size: " << nread << std::endl;

        int ret = ikcp_input(client->kcp, buf->base, nread);
        if (ret == IKCP_CMD_ACK)
        {
            return;
        }

        int32_t msg_len = ikcp_peeksize(client->kcp);
        if (msg_len <= 0)
        {
            // 消息没有接收完整
            return;
        }

        char *msg = (char *)malloc(msg_len);
        int bytes_recv = ikcp_recv(client->kcp, msg, msg_len);
        if (bytes_recv > 0)
        {
            std::cout << "Message: \n\n"
                      << msg << "\n\n"
                      << msg_len << std::endl;
            free(msg);
        }
        else
        {
            std::cerr << "Message receive failed!" << std::endl;
        }
    }

    void on_send(uv_udp_send_t *req, int status)
    {
        if (status < 0)
        {
            close();
        }
    }

    int init(uint32_t conv_id)
    {
        client = (zclient *)malloc(sizeof(zclient));
        client->loop = uv_default_loop();
        client->kcp = ikcp_create(conv_id, client);
        client->kcp->output = output;

        client->kcp_timer = (uv_timer_t *)malloc(sizeof(uv_timer_t));
        uv_timer_init(client->loop, client->kcp_timer);

        return 0;
    }

    void on_kcp_timer(uv_timer_t *handle)
    {
        if (client != nullptr)
        {
            ikcp_update(client->kcp, clock());
        }
    }

    int connect(char *address, int port)
    {
        if (client == nullptr)
        {
            return -1;
        }

        sockaddr_in recv_addr;
        uv_ip4_addr("0.0.0.0", 1234, &recv_addr);
        client->handle = (uv_udp_t *)malloc(sizeof(uv_udp_t));
        uv_udp_init(client->loop, client->handle);
        uv_udp_bind(client->handle, (const sockaddr *)&recv_addr, 0);
        uv_udp_recv_start(client->handle, alloc_cb, on_recv);

        uv_ip4_addr(address, port, &client->remote_addr);
        uv_udp_connect(client->handle, (const sockaddr *)&client->remote_addr);

        uv_timer_start(client->kcp_timer, on_kcp_timer, 40, 40);

        return 0;
    }

    int run()
    {
        if (client == nullptr)
        {
            return -1;
        }

        return uv_run(client->loop, UV_RUN_DEFAULT);
    }

    int send(char *message, size_t size)
    {
        if (client == nullptr)
        {
            return -1;
        }

        return ikcp_send(client->kcp, message, size);
    }

    // for test
    void on_pipe_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
    {
        if (nread < 0)
        {
            uv_close((uv_handle_t *)client->stdin_pipe, nullptr);
        }
        else
        {
            std::cout << send(buf->base, nread) << std::endl;
        }

        if (buf->base != nullptr)
        {
            free(buf->base);
        }
    }
    void open_stdin_pipe()
    {
        client->stdin_pipe = (uv_pipe_t *)malloc(sizeof(uv_pipe_t));
        uv_pipe_init(client->loop, client->stdin_pipe, 0);
        uv_pipe_open(client->stdin_pipe, 0);
        uv_read_start((uv_stream_t *)client->stdin_pipe, alloc_cb, on_pipe_read);
    }
}

int main()
{
    char addr[] = "127.0.0.1";
    znet::init(996);
    znet::connect(addr, 1234);
    znet::open_stdin_pipe();
    znet::run();
}