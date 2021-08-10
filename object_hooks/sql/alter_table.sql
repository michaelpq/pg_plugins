--
-- Tests for object access hook with ALTER TABLE
--

LOAD 'object_hooks';

-- Create a heap2 table am handler with heapam handler
CREATE ACCESS METHOD heap2 TYPE TABLE HANDLER heap_tableam_handler;

CREATE TABLE object_hook_tab (a int PRIMARY KEY, b int);
ALTER TABLE object_hook_tab SET UNLOGGED;
ALTER TABLE object_hook_tab SET LOGGED;
ALTER TABLE object_hook_tab SET LOGGED; -- no-op
ALTER TABLE object_hook_tab SET TABLESPACE pg_default; -- no-op
ALTER TABLE object_hook_tab SET ACCESS METHOD heap2;
ALTER TABLE object_hook_tab SET ACCESS METHOD heap;
ALTER TABLE object_hook_tab SET ACCESS METHOD heap; -- no-op
ALTER TABLE object_hook_tab ADD column c int;
ALTER TABLE object_hook_tab DROP column c;
CREATE SCHEMA popo;
ALTER TABLE object_hook_tab SET SCHEMA popo;
ALTER TABLE popo.object_hook_tab SET SCHEMA public;

-- Clean up
DROP ACCESS METHOD heap2;
DROP TABLE object_hook_tab;
