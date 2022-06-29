#include "ikcp.h"
#include "uv.h"
#include "common.h"
#include <iostream>

namespace znet
{
    struct zserver
    {
        ikcpcb *kcp;
        uv_loop_t *loop;
        uv_udp_t *handle;
        uv_timer_t *kcp_timer;
        uv_udp_send_t *send_req;
    };

    zserver *server;
    sockaddr remote_endpoint;

    void on_send(uv_udp_send_t *req, int status);

    int output(const char *buf, int len, ikcpcb *kcp, void *user)
    {
        if (server->send_req == nullptr)
        {
            server->send_req = (uv_udp_send_t *)malloc(sizeof(uv_udp_send_t));
        }

        uv_buf_t uv_buf = uv_buf_init((char *)malloc(len), len);
        memcpy(uv_buf.base, buf, len);

        return uv_udp_send(server->send_req, server->handle, &uv_buf, 1, &remote_endpoint, on_send);
    }

    void on_close(uv_handle_t *handle)
    {
        if (server != nullptr)
        {
            free(server);
            server = nullptr;
        }
    }

    void close()
    {
        if (server == nullptr)
        {
            return;
        }

        uv_close((uv_handle_t *)server->handle, on_close);
    }

    void on_send(uv_udp_send_t *req, int status)
    {
        if (status < 0)
        {
            close();
        }
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

        int ret = ikcp_input(server->kcp, buf->base, nread);
        if (ret == IKCP_CMD_ACK)
        {
            return;
        }

        int32_t msg_len = ikcp_peeksize(server->kcp);
        if (msg_len <= 0)
        {
            // 消息没有接收完整
            return;
        }

        char *msg = (char *)malloc(msg_len);
        int bytes_recv = ikcp_recv(server->kcp, msg, msg_len);
        if (bytes_recv > 0)
        {
            std::cout << "Message: \n\n"
                      << msg << "\n\n"
                      << msg_len << std::endl;

            memcpy(&remote_endpoint, addr, sizeof(sockaddr));

            ikcp_send(server->kcp, msg, msg_len);

            free(msg);
        }
        else
        {
            std::cerr << "Message receive failed!" << std::endl;
        }
    }

    void on_kcp_timer(uv_timer_t *handle)
    {
        if (server != nullptr)
        {
            ikcp_update(server->kcp, clock());
        }
    }

    int init(int port)
    {
        server = (zserver *)malloc(sizeof(zserver));
        server->loop = uv_default_loop();
        server->kcp = ikcp_create(996, (void *)server);
        server->kcp->output = output;
        ikcp_nodelay(server->kcp, 0, 100, 1, 0);
        ikcp_wndsize(server->kcp, 128, 128);

        sockaddr_in recv_addr;
        uv_ip4_addr("0.0.0.0", port, &recv_addr);
        server->handle = (uv_udp_t *)malloc(sizeof(uv_udp_t));
        uv_udp_init(server->loop, server->handle);
        uv_udp_bind(server->handle, (const sockaddr *)&recv_addr, 0);
        uv_udp_recv_start(server->handle, alloc_cb, on_recv);

        server->kcp_timer = (uv_timer_t *)malloc(sizeof(uv_timer_t));
        uv_timer_init(server->loop, server->kcp_timer);
        uv_timer_start(server->kcp_timer, on_kcp_timer, 40, 40);

        return 0;
    }

    int run()
    {
        if (server == nullptr)
        {
            return -1;
        }

        return uv_run(server->loop, UV_RUN_DEFAULT);
    }
} // namespace znet

int main()
{
    znet::init(1234);
    znet::run();
}