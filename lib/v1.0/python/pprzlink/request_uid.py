class RequestUIDFactory:
    _generator = None

    @classmethod
    def _unique_id_generator(cls):
        import os
        pid = os.getpid()

        sequence_number = 0
        while True:
            sequence_number = sequence_number + 1
            yield f'{pid}_{sequence_number}'

    @classmethod
    def generate_uid(cls):
        if cls._generator is None:
            cls._generator = cls._unique_id_generator()
        return next(cls._generator)
