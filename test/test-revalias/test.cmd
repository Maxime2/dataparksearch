#skip !0 testenv DPS_TEST_ROOT
#skip !0 testenv DPS_TEST_DIR
#skip !0 testenv DPS_TEST_DBADDR0
#skip !0 testenv DPS_SHARE_DIR
#skip !0 testenv INDEXER
skip !0 exec $(INDEXER) -Echeck  $(DPS_TEST_DIR)/indexer0.conf >> $(DPS_TEST_LOG) 2>&1

# Prepare table structure
fail 20 exec $(INDEXER) -Edrop   $(DPS_TEST_DIR)/indexer0.conf >> $(DPS_TEST_LOG) 2>&1
fail !0 exec $(INDEXER) -Ecreate $(DPS_TEST_DIR)/indexer0.conf >> $(DPS_TEST_LOG) 2>&1

# Index with ReversAlias
fail !0 exec $(INDEXER) -Eindex  $(DPS_TEST_DIR)/indexer0.conf >> $(DPS_TEST_LOG) 2>&1

# Check dict contents
fail !0 exec $(INDEXER) -Esqlmon $(DPS_TEST_DIR)/indexer0.conf < $(DPS_TEST_DIR)/dict.tst > $(DPS_TEST_DIR)/dict.rej 2>&1
fail !0 mdiff $(DPS_TEST_DIR)/dict.rej $(DPS_TEST_DIR)/dict.res
fail !0 exec rm -f $(DPS_TEST_DIR)/dict.rej

# Check url contents
fail !0 exec $(INDEXER) -Esqlmon $(DPS_TEST_DIR)/indexer0.conf < $(DPS_TEST_DIR)/url.tst > $(DPS_TEST_DIR)/url0.rej 2>&1
fail !0 mdiff $(DPS_TEST_DIR)/url0.rej $(DPS_TEST_DIR)/url0.res
fail !0 exec rm -f $(DPS_TEST_DIR)/url0.rej

# Clear database
fail !0 exec $(INDEXER) -Cw $(DPS_TEST_DIR)/indexer0.conf >> $(DPS_TEST_LOG) 2>&1

# Index without ReverseAlias
fail !0 exec $(INDEXER) -Eindex  $(DPS_TEST_DIR)/indexer1.conf >> $(DPS_TEST_LOG) 2>&1

# Check dict contents: must be the same
fail !0 exec $(INDEXER) -Esqlmon $(DPS_TEST_DIR)/indexer1.conf < $(DPS_TEST_DIR)/dict.tst > $(DPS_TEST_DIR)/dict.rej 2>&1
fail !0 mdiff $(DPS_TEST_DIR)/dict.rej $(DPS_TEST_DIR)/dict.res
fail !0 exec rm -f $(DPS_TEST_DIR)/dict.rej

# Check url contens
fail !0 exec $(INDEXER) -Esqlmon $(DPS_TEST_DIR)/indexer1.conf < $(DPS_TEST_DIR)/url.tst > $(DPS_TEST_DIR)/url1.rej 2>&1
fail !0 mdiff $(DPS_TEST_DIR)/url1.rej $(DPS_TEST_DIR)/url1.res
fail !0 exec rm -f $(DPS_TEST_DIR)/url1.rej

# Drop tables
pass 0 exec  $(INDEXER) -Edrop $(DPS_TEST_DIR)/indexer0.conf >> $(DPS_TEST_LOG) 2>&1
