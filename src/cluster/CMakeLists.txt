add_library(clio_cluster)

target_sources(clio_cluster PRIVATE ClioNode.cpp ClusterCommunicationService.cpp)

target_link_libraries(clio_cluster PRIVATE clio_util clio_data)
