I want to add a limitation for simultaneous cache loading in a cluster, so only one node at a time can load cache.
We have ClusterCommunicationService for that.

Implementation steps:
- [ ] Add methods startLoading() and isCurrentlyLoading() to LedgerCache and its interface. setFull() should mark loading as finished.
- [ ] CacheLoader should call startLoading() in load() method if it didn't load the cache from file.
- [ ] Create CacheLoadingState containing a reference to cache and atomic bool whether the cache is allowed to be loaded.
  It should have methods allowCacheLoading() and waitForCacheLoadingToBeAllowed()
- [ ] Add CacheLoadingState to CacheLoader. If it was not able to load cache from file it should wait for loading to be allowed.
- [ ] Add CacheLoadingState to ClusterCommunicationService. ClioNode should have a field cacheIsCurrentlyLoading.
- [ ] Add CacheLoadingDecider which will among all nodes take all which doesn't have full cache and if there is no one loading it allow to load for the first node sorted by uuid. Similar to WriterDecider.

After each step you should update/add tests and make sure the project compiles and tests are passing.
