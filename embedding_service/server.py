from concurrent import futures

import grpc
from sentence_transformers import SentenceTransformer

import embedding_pb2 as pb
import embedding_pb2_grpc as pb_grpc


MODEL_NAME = "intfloat/multilingual-e5-small"
EMBEDDING_DIM = 384
ADDRESS = "127.0.0.1:50051"


class EmbeddingService(pb_grpc.EmbeddingServiceServicer):
    def __init__(self):
        # Загружаем модель один раз при создании сервиса.
        self.model = SentenceTransformer(MODEL_NAME, device="cpu")
        self.model.max_seq_length = 512

        if self.model.get_sentence_embedding_dimension() != EMBEDDING_DIM:
            raise RuntimeError("Unexpected embedding dimension")

    def EmbedQuery(self, request, context):
        if not request.query.strip():
            context.abort(
                grpc.StatusCode.INVALID_ARGUMENT,
                "Query must not be empty",
            )

        vector = self.model.encode(
            "query: " + request.query,
            normalize_embeddings=True,
            show_progress_bar=False,
        )

        return pb.Embedding(values=vector.tolist())

    def EmbedPassages(self, request, context):
        if not request.passages:
            return pb.EmbedPassagesResponse()

        vectors = self.model.encode(
            ["passage: " + text for text in request.passages],
            batch_size=16,
            normalize_embeddings=True,
            show_progress_bar=False,
        )

        return pb.EmbedPassagesResponse(
            embeddings=[
                pb.Embedding(values=vector.tolist())
                for vector in vectors
            ]
        )


if __name__ == "__main__":
    print(f"Loading {MODEL_NAME}...", flush=True)
    service = EmbeddingService()

    # Пока обрабатываем один запрос к модели за раз.
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=1))
    pb_grpc.add_EmbeddingServiceServicer_to_server(service, server)

    if server.add_insecure_port(ADDRESS) == 0:
        raise RuntimeError(f"Cannot listen on {ADDRESS}")

    server.start()
    print(f"Embedding service listening on {ADDRESS}", flush=True)

    try:
        server.wait_for_termination()
    except KeyboardInterrupt:
        server.stop(5).wait()