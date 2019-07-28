-- Event triggers

CREATE OR REPLACE FUNCTION log_any_command()
  RETURNS event_trigger
  LANGUAGE plpgsql
  AS $$
BEGIN
  RAISE NOTICE 'command % for event %', tg_tag, tg_event;
END;
$$;
CREATE EVENT TRIGGER ddl_start ON ddl_command_start
   EXECUTE FUNCTION log_any_command();
CREATE EVENT TRIGGER ddl_end ON ddl_command_end
   EXECUTE FUNCTION log_any_command();
CREATE EVENT TRIGGER ddl_drop ON sql_drop
   EXECUTE FUNCTION log_any_command();
CREATE EVENT TRIGGER ddl_rewrite ON table_rewrite
   EXECUTE FUNCTION log_any_command();
