FROM ghcr.io/oran-testing/components_base AS builder

WORKDIR /spoofer

COPY . .
RUN sed -i 's|#include "srsran/srslog/sink.h"|#include "srsran/srslog/sink.h"\n#undef stdout\n#undef stderr|g'   \
    /spoofer/lib/src/srslog/sinks/stream_sink.h && \
    sed -i '21s|.*|#include <sys/time.h>|g' /spoofer/lib/src/phy/io/netsource.c && \
    sed -i -e '31,39s|.*||g' -e 's|#include <execinfo.h>||g' /spoofer/lib/src/common/backtrace.c && \
    find /spoofer/ -type f -exec sed -i 's|uint |unsigned int |g' {} + && \
    sed -i -e '28,30s|.*||g' -e '61s|.*||g' /spoofer/lib/include/srsran/upper/ipv6.h

RUN mkdir -p build && rm -rf build/*

WORKDIR /spoofer/build
RUN cmake -DENABLE_ZEROMQ=ON .. && \
    make -j$(nproc) && \
    make install

FROM alpine:latest
ENV PYTHONUNBUFFERED=1
RUN apk add --no-cache libstdc++ ca-certificates && update-ca-certificates || true

COPY --from=builder /usr/local/bin/ssb_spoofer /usr/local/bin/ssb_spoofer

ENV ARGS=""

CMD ["sh", "-c", "/usr/local/bin/ssb_spoofer --config /spoofer.yaml $ARGS"]