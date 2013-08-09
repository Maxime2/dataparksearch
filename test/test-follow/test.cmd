#skip !0 testenv DPS_TEST_ROOT
#skip !0 testenv DPS_TEST_DIR
#skip !0 testenv DPS_TEST_DBADDR0
#skip !0 testenv DPS_SHARE_DIR
#skip !0 testenv INDEXER
skip !0 exec $(INDEXER) -Echeck  $(DPS_TEST_DIR)/indexer.conf >> $(DPS_TEST_LOG) 2>&1

fail 20 exec $(INDEXER) -Edrop   $(DPS_TEST_DIR)/indexer.conf >> $(DPS_TEST_LOG) 2>&1
fail !0 exec $(INDEXER) -Ecreate $(DPS_TEST_DIR)/indexer.conf >> $(DPS_TEST_LOG) 2>&1
fail !0 exec $(INDEXER) -Eindex -v6 $(DPS_TEST_DIR)/indexer.conf >> $(DPS_TEST_LOG) 2>&1
fail !0 exec $(INDEXER) -Esqlmon $(DPS_TEST_DIR)/indexer.conf < $(DPS_TEST_DIR)/query.tst > $(DPS_TEST_DIR)/query.rej 2>&1
fail !0 mdiff $(DPS_TEST_DIR)/query.rej $(DPS_TEST_DIR)/query.res
fail !0 exec rm -f $(DPS_TEST_DIR)/query.rej
pass 0 exec  $(INDEXER) -Edrop $(DPS_TEST_DIR)/indexer.conf >> $(DPS_TEST_LOG) 2>&1
