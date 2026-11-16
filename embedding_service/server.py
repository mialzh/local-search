from concurrent import futures
from hashlib import sha256

import grpc

import embedding_pb2 as pb
import embedding_pb2_grpc as pb_grpc

EMBEDDING_DIM = 384


def make_embedding(text: str) -> pb.Embedding:
    digest = sha256(text.encode("utf-8")).digest()
    position = int.from_bytes(digest[:2], "big") % EMBEDDING_DIM
    values = [0.0] * EMBEDDING_DIM
    values[position] = 1.0
    return pb.Embedding(values=values)


class EmbeddingService(pb_grpc.EmbeddingServiceServicer):
    def EmbedQuery(self, request, context):
        return make_embedding(request.query)

    def EmbedPassages(self, request, context):
        return pb.EmbedPassagesResponse(
            embeddings=[make_embedding(text) for text in request.passages]
        )


if __name__ == "__main__":
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=4))
    pb_grpc.add_EmbeddingServiceServicer_to_server(EmbeddingService(), server)
    server.add_insecure_port("127.0.0.1:50051")
    server.start()
    print("Embedding service listening on 127.0.0.1:50051")
    server.wait_for_termination()