
extern "C" {
#include "quic.h"
}

#include "Http3ContextData.h"
#include "Http3ResponseData.h"

namespace uWS {
    struct Http3Context {
        static Http3Context *create(us_loop_t *loop, us_quic_socket_context_options_t options) {
            //return nullptr;

            printf("Creating context now\n");

            /* Create quic socket context (assumes h3 for now) */
            auto *context = us_create_quic_socket_context(loop, options, sizeof(Http3ContextData)); // sizeof(Http3ContextData)

            /* Specify application callbacks */
            us_quic_socket_context_on_stream_data(context, [](us_quic_stream_t *s, char *data, int length) {

                // we don't have a way to know EOF?
                Http3ResponseData *responseData = (Http3ResponseData *) us_quic_stream_ext(s);
                
                if (responseData->onData) {
                    responseData->onData({data, length}, true);
                }

                //printf("Body length is: %d\n", length);
            });
            us_quic_socket_context_on_stream_open(context, [](us_quic_stream_t *s, int is_client) {
                //printf("Stream opened!\n");

                // inplace initialize Http3ResponseData here

                new (us_quic_stream_ext(s)) Http3ResponseData();
            });
            us_quic_socket_context_on_close(context, [](us_quic_socket_t *s) {
                printf("Disconnected from on_close in uws!\n");

            });
            us_quic_socket_context_on_stream_writable(context, [](us_quic_stream_t *s) {
                Http3ResponseData *responseData = (Http3ResponseData *) us_quic_stream_ext(s);
                // responseData->onWritable();

                int written = us_quic_stream_write(s, responseData->buffer.data() + responseData->bufferOffset, responseData->buffer.length() - responseData->bufferOffset);

                //printf("wrote %d bytes in writable callback\n", written);

                // this whole thing should use the BackpressureBuffer class
                responseData->bufferOffset += written;//responseData->buffer.substr(written);

                printf("remaingin bytes: %ld\n", responseData->buffer.length() - responseData->bufferOffset);

                if (responseData->buffer.length() - responseData->bufferOffset == 0) {
                    printf("wrote until end, shutting down now!\n");
                    us_quic_stream_shutdown(s);
                    us_quic_stream_close(s);
                }

                //printf("stream is now writable!\n");
                
            });
            us_quic_socket_context_on_stream_headers(context, [](us_quic_stream_t *s) {

                Http3ContextData *contextData = (Http3ContextData *) us_quic_socket_context_ext(us_quic_socket_context(us_quic_stream_socket(s)));

                Http3Request *req = nullptr;

                std::string_view upperCasedMethod = req->getHeader(":method");
                std::string_view path = req->getHeader(":path");
                
                contextData->router.getUserData() = {(Http3Response *) s, (Http3Request *) nullptr};
                contextData->router.route(upperCasedMethod, path);

            });
            us_quic_socket_context_on_open(context, [](us_quic_socket_t *s, int is_client) {
                printf("Connection established!\n");
            });
            us_quic_socket_context_on_stream_close(context, [](us_quic_stream_t *s) {

                Http3ResponseData *responseData = (Http3ResponseData *) us_quic_stream_ext(s);
                
                if (responseData->onAborted) {
                    responseData->onAborted();
                }

                responseData->~Http3ResponseData();
            });

            return (Http3Context *) context;

            // call init here after setting the ext to Http3ContextData
        }

        us_quic_listen_socket_t *listen() {
            /* The listening socket is the actual UDP socket used */
            us_quic_listen_socket_t *listen_socket = us_quic_socket_context_listen((us_quic_socket_context_t *) this, "::", 9004, sizeof(Http3ResponseData));

            printf("Listen socket is: %p\n", listen_socket);

            return listen_socket;
        }

        void init() {
            // set all callbacks here



            Http3ContextData *contextData = (Http3ContextData *) us_quic_socket_context_ext((us_quic_socket_context_t *) this);

            printf("init: %p\n", contextData);

            new (contextData) Http3ContextData();

        }

        // generic for get, post, any, etc
        void onHttp(std::string method, std::string path, MoveOnlyFunction<void(Http3Response *, Http3Request *)> &&cb) {
            // modifies the router we own as part of Http3ContextData, used in callbacks set in init
        
            Http3ContextData *contextData = (Http3ContextData *) us_quic_socket_context_ext((us_quic_socket_context_t *) this);

            /* Todo: This is ugly, fix */
            std::vector<std::string> methods;
            if (method == "*") {
                methods = contextData->router.upperCasedMethods; //bug! needs to be upper cased!
                // router.upperCasedMethods;
            } else {
                methods = {method};
            }

            contextData->router.add(methods, path, [handler = std::move(cb)](HttpRouter<Http3ContextData::RouterData> *router) mutable {

                Http3ContextData::RouterData &routerData = router->getUserData();

                handler(routerData.res, routerData.req);

                return true;
            });
        }
    };
}