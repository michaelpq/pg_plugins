-- Sanity check for archive_get_size
SELECT archive_get_size('../no_parent'); -- error
SELECT archive_get_size('/no_absolute'); -- error
