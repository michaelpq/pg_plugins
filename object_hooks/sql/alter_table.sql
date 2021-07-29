--
-- Tests for object access hook with ALTER TABLE
--

LOAD 'object_hooks';

-- Create a heap2 table am handler with heapam handler
CREATE ACCESS METHOD heap2 TYPE TABLE HANDLER heap_tableam_handler;

CREATE TABLE object_hook_tab (a int PRIMARY KEY, b int);
ALTER TABLE object_hook_tab SET UNLOGGED;
ALTER TABLE object_hook_tab SET LOGGED;
ALTER TABLE object_hook_tab SET TABLESPACE pg_default;
ALTER TABLE object_hook_tab SET ACCESS METHOD heap2;
ALTER TABLE object_hook_tab SET ACCESS METHOD heap;

-- Clean up
DROP ACCESS METHOD heap2;
DROP TABLE object_hook_tab;
