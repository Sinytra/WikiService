ALTER TABLE project ADD COLUMN search_vector tsvector;

UPDATE project SET search_vector = to_tsvector('english', id || ' ' || name);

CREATE INDEX project_search_vector_idx ON project USING gin(search_vector);

CREATE OR REPLACE FUNCTION update_search_vector()
    RETURNS TRIGGER AS $$
BEGIN
    NEW.search_vector := to_tsvector('english', NEW.id || ' ' || NEW.name);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER set_search_vector
    BEFORE INSERT ON project
    FOR EACH ROW
EXECUTE FUNCTION update_search_vector();