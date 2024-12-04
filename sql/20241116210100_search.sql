UPDATE project SET search_vector = to_tsvector('english', id || ' ' || name);
