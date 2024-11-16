ALTER TABLE mod ADD COLUMN search_vector tsvector;

UPDATE mod SET search_vector = to_tsvector('english', id || ' ' || name);

CREATE INDEX mod_search_vector_idx ON mod USING gin(search_vector);

CREATE OR REPLACE FUNCTION update_search_vector()
    RETURNS TRIGGER AS $$
BEGIN
    NEW.search_vector := to_tsvector('english', NEW.id || ' ' || NEW.name);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER set_search_vector
    BEFORE INSERT ON mod
    FOR EACH ROW
EXECUTE FUNCTION update_search_vector();