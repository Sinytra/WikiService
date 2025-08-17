CREATE TABLE access_key
(
    id         bigserial PRIMARY KEY,

    name       text                                   NOT NULL,
    value      text                                   NOT NULL,
    user_id    text REFERENCES user_ (id) ON DELETE CASCADE,
    expires_at timestamp(3),

    created_at timestamp(3) default CURRENT_TIMESTAMP NOT NULL,

    UNIQUE (value)
);
