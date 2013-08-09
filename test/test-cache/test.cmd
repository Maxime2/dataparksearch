#skip !0 testenv DPS_TEST_ROOT
#skip !0 testenv DPS_TEST_DIR
#skip !0 testenv DPS_TEST_DBADDR0
#skip !0 testenv DPS_SHARE_DIR
#skip !0 testenv INDEXER
skip !0 exec $(INDEXER) -Echeck  $(DPS_TEST_DIR)/indexer.conf >> $(DPS_TEST_LOG) 2>&1

fail !0 exec rm -rf $(DPS_TEST_DIR)/var/
fail !0 exec mkdir $(DPS_TEST_DIR)/var
fail !0 exec mkdir $(DPS_TEST_DIR)/var/cache
fail !0 exec mkdir $(DPS_TEST_DIR)/var/splitter
fail !0 exec mkdir $(DPS_TEST_DIR)/var/store
fail !0 exec mkdir $(DPS_TEST_DIR)/var/tree
fail !0 exec mkdir $(DPS_TEST_DIR)/var/url
fail 20 exec $(INDEXER) -Edrop   $(DPS_TEST_DIR)/indexer.conf >> $(DPS_TEST_LOG) 2>&1
fail !0 exec $(INDEXER) -Ecreate $(DPS_TEST_DIR)/indexer.conf >> $(DPS_TEST_LOG) 2>&1
fail !0 exec echo "INSERT INTO categories (rec_id,path,name)values(2,'01','test');" | $(INDEXER) -Q $(DPS_TEST_DIR)/indexer.conf >> $(DPS_TEST_LOG) 2>&1
fail !0 exec echo "INSERT INTO categories (rec_id,path,name)values(3,'02','testif');" | $(INDEXER) -Q $(DPS_TEST_DIR)/indexer.conf >> $(DPS_TEST_LOG) 2>&1
fail !0 exec $(INDEXER) -Eindex  -v5 $(DPS_TEST_DIR)/indexer.conf >> $(DPS_TEST_LOG) 2>&1
fail !0 exec $(INDEXER) -Eindex  -amv5 $(DPS_TEST_DIR)/indexer.conf >> $(DPS_TEST_LOG) 2>&1
#fail !0 exec $(INDEXER) -Eindex  -v5 $(DPS_TEST_DIR)/indexer.conf > $(DPS_TEST_DIR)/indexer.log 2>&1
fail !0 exec $(INDEXER) -TWv5      $(DPS_TEST_DIR)/indexer.conf >> $(DPS_TEST_LOG) 2>&1
fail !0 exec $(INDEXER) -Esqlmon $(DPS_TEST_DIR)/indexer.conf < $(DPS_TEST_DIR)/query.tst > $(DPS_TEST_DIR)/query.rej 2>&1
fail !0 exec $(SEARCH) "body1" > $(DPS_TEST_DIR)/search.rej 2>&1
fail !0 exec $(SEARCH) "Wall" > $(DPS_TEST_DIR)/search2.rej 2>&1
fail !0 exec $(SEARCH) "body1&rm=2" > $(DPS_TEST_DIR)/search3.rej 2>&1
fail !0 exec $(SEARCH) "Wall&rm=2" > $(DPS_TEST_DIR)/search4.rej 2>&1
fail !0 exec $(SEARCH) "bosch" > $(DPS_TEST_DIR)/search5.rej 2>&1
fail !0 exec $(SEARCH) "@twitter #trend l'orex" > $(DPS_TEST_DIR)/search6.rej 2>&1
#fail !0 exec $(SEARCH) "Wall+%22Street+Journal%22&sp=1&ps=15&m=near" > $(DPS_TEST_DIR)/search8.rej 2>&1


#pass 0 exec  $(INDEXER) -Edrop $(DPS_TEST_DIR)/indexer.conf >> $(DPS_TEST_LOG) 2>&1

# indexing from stored test
fail !0 exec $(INDEXER) -Eindex  -v5 -qaB $(DPS_TEST_DIR)/indexer.conf >> $(DPS_TEST_LOG) 2>&1
fail !0 exec $(INDEXER) -TW      $(DPS_TEST_DIR)/indexer.conf >> $(DPS_TEST_LOG) 2>&1
fail !0 exec $(INDEXER) -Esqlmon $(DPS_TEST_DIR)/indexer.conf < $(DPS_TEST_DIR)/query.tst > $(DPS_TEST_DIR)/query-2.rej 2>&1
fail !0 exec $(SEARCH) body1 > $(DPS_TEST_DIR)/search-2.rej 2>&1
fail !0 exec $(SEARCH) Wall > $(DPS_TEST_DIR)/search2-2.rej 2>&1


# check all results now
fail !0 mdiff $(DPS_TEST_DIR)/query.rej $(DPS_TEST_DIR)/query.res
fail !0 exec rm -f $(DPS_TEST_DIR)/query.rej

fail !0 mdiff $(DPS_TEST_DIR)/search.rej $(DPS_TEST_DIR)/search.res
fail !0 exec rm -f $(DPS_TEST_DIR)/search.rej

fail !0 mdiff $(DPS_TEST_DIR)/search2.rej $(DPS_TEST_DIR)/search2.res
fail !0 exec rm -f $(DPS_TEST_DIR)/search2.rej


fail !0 mdiff $(DPS_TEST_DIR)/query-2.rej $(DPS_TEST_DIR)/query.res
fail !0 exec rm -f $(DPS_TEST_DIR)/query-2.rej

fail !0 mdiff $(DPS_TEST_DIR)/search-2.rej $(DPS_TEST_DIR)/search.res
fail !0 exec rm -f $(DPS_TEST_DIR)/search-2.rej

fail !0 mdiff $(DPS_TEST_DIR)/search2-2.rej $(DPS_TEST_DIR)/search2.res
fail !0 exec rm -f $(DPS_TEST_DIR)/search2-2.rej


fail !0 mdiff $(DPS_TEST_DIR)/search3.rej $(DPS_TEST_DIR)/search3.res
fail !0 exec rm -f $(DPS_TEST_DIR)/search3.rej

fail !0 mdiff $(DPS_TEST_DIR)/search4.rej $(DPS_TEST_DIR)/search4.res
fail !0 exec rm -f $(DPS_TEST_DIR)/search4.rej

fail !0 mdiff $(DPS_TEST_DIR)/search5.rej $(DPS_TEST_DIR)/search5.res
fail !0 exec rm -f $(DPS_TEST_DIR)/search5.rej

fail !0 mdiff $(DPS_TEST_DIR)/search6.rej $(DPS_TEST_DIR)/search6.res
fail !0 exec rm -f $(DPS_TEST_DIR)/search6.rej


#fail !0 exec  $(INDEXER) -Edrop $(DPS_TEST_DIR)/indexer.conf >> $(DPS_TEST_LOG) 2>&1
pass 0 exec rm -rf $(DPS_TEST_DIR)/var/

