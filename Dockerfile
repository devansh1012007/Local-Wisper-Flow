FROM python:3.11-slim

WORKDIR /app

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1

COPY pyproject.toml README.md ./
COPY src ./src

RUN pip install --no-cache-dir -e ".[dev]"

EXPOSE 50051

CMD ["python", "-m", "vision_sst", "--port", "50051"]
