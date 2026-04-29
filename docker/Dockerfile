# Stage 1: builder
# Compiles liblecerf.so for musl/alpine, then pip-builds the lecerf wheel.
FROM python:3.12-alpine AS builder

RUN apk add --no-cache --virtual .build-deps \
        gcc musl-dev cmake make linux-headers

WORKDIR /src

# Copy C sources first (layer cache: C rarely changes less often than Python)
COPY CMakeLists.txt /src/
COPY include/ /src/include/
COPY src/ /src/src/
COPY tools/ /src/tools/
COPY tests/ /src/tests/

# Compile liblecerf.so for musl via cmake
# LECERF_BUILD_PYTHON=ON skips the CLI executable and test subdirectory
RUN cmake -B /src/build \
          -DCMAKE_BUILD_TYPE=Release \
          -DLECERF_BUILD_PYTHON=ON \
    && cmake --build /src/build --target liblecerf --parallel

# Copy Python package sources (without the Windows DLL — excluded by .dockerignore)
COPY python/ /src/python/

# Inject the freshly built .so into the package directory before wheel build
RUN cp /src/build/liblecerf.so /src/python/lecerf/liblecerf.so

# Build the wheel
RUN pip install --no-cache-dir build \
 && python -m build --wheel --outdir /wheels /src/python

# Remove heavy build tools from the builder stage to keep layer accounting honest
RUN apk del .build-deps


# Stage 2: runtime
# Bare python:3.12-alpine + the wheel + pytest + runner entry-point.
FROM python:3.12-alpine

COPY --from=builder /wheels/lecerf-*.whl /tmp/

# Install wheel + pytest, then trim all fat in the SAME RUN layer so no bytes
# are retained in the layer cache.
RUN pip install --no-cache-dir /tmp/lecerf-*.whl pytest \
 && rm -rf /usr/local/lib/python3.12/site-packages/pip \
 && rm -rf /usr/local/lib/python3.12/site-packages/setuptools \
 && find /usr/local/lib/python3.12 -type d -name __pycache__ -exec rm -rf {} + 2>/dev/null; true \
 && find /usr/local/lib/python3.12 -name '*.pyc' -delete 2>/dev/null; true \
 && rm -rf /tmp/*.whl /root/.cache /var/cache/apk/*

COPY docker/runner.py /usr/local/bin/lecerf-runner
RUN chmod +x /usr/local/bin/lecerf-runner

ENTRYPOINT ["/usr/local/bin/lecerf-runner"]
CMD ["--help"]
