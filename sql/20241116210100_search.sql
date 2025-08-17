UPDATE project SET search_vector = to_tsvector('simple', id || ' ' || name);
