add_library(clio_cluster)

target_sources(
  clio_cluster PRIVATE Backend.cpp ClioNode.cpp ClusterCommunicationService.cpp Metrics.cpp WriterDecider.cpp
)

target_link_libraries(clio_cluster PRIVATE clio_util clio_data)
