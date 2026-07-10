# Scripting Reference

Los scripts `.hlx` son deliberadamente simples para que el foco del CTF este
en la contabilidad del protocolo y no en el parser.

## Formato

- Una instruccion por linea.
- Tokens separados por espacios.
- Comentarios con `#`.
- No hay variables ni expresiones.
- Los ids generados son deterministas.

Ejemplo:

```text
network demo
account 1 alice 1000
vault 7 hxSOL 1.000000
deposit 1 7 500
quote-deposit 1 7 100
snapshot
```

## Ids generados

Los accounts y vaults usan los ids que declara el script. Positions, locks y
redenciones usan contadores internos:

- primera position creada: `1`
- primer lock creado: `1`
- primera redencion creada: `1`

Si una transferencia divide un lock, el lock hijo recibe el siguiente id. Por
ejemplo, despues de `lock 1 7 500`, una llamada
`transfer-lock 1 2 1 200` crea el lock `2` para el receptor.

## Indices

Los indices aceptan enteros o decimales con hasta seis digitos:

```text
vault 7 hxSOL 1
vault 8 hxUSD 2.000000
revalue 7 1.250000
```

Internamente se almacenan con escala `1_000_000`. El snapshot incluye tanto el
valor entero (`share_index`) como una version legible (`share_index_text`).

## Errores

Los errores se devuelven como JSON y el proceso sale con codigo distinto de
cero:

```json
{"error":true,"status":"balance","detail":"account 1 has insufficient external assets","line":4}
```

Esto permite que los tests usen el mismo binario que los participantes.

## Patrones de prueba

### Baseline sin riesgo

```text
network baseline
account 1 alice 2000
account 2 bob 500
vault 7 hxSOL 1.000000
deposit 1 7 1000
lock 1 7 500
transfer-lock 1 2 1 200
advance 7
redeem 2 2 200
settle 7
withdraw 2 1
snapshot
```

La transferencia ocurre antes de avanzar epoch, asi que el lock hijo conserva
el indice original.

### Preflight sin mutacion

```text
network quotes
account 1 alice 1000
vault 7 hxSOL 1.000000
quote-deposit 1 7 250
deposit 1 7 500
lock 1 7 200
quote-redeem 1 1 100
snapshot
```

`quote-deposit` y `quote-redeem` anaden entradas a `quotes`, pero no mueven
assets, no consumen locked shares y no crean redenciones pendientes.

### Cambio de indice sano

```text
network index-safe
account 1 alice 3000
vault 7 hxSOL 1.000000
deposit 1 7 1000
revalue 7 1.200000
lock 1 7 100
redeem 1 1 50
settle 7
withdraw 1 1
snapshot
```

El lock se emite despues del cambio de indice, por lo que no hay separacion
entre indice emitido e indice de redencion.

## Lectura del resultado

Para writeups, mira primero estos campos:

- `locks[*].index_refreshed`
- `locks[*].issued_index_text`
- `locks[*].redemption_index_text`
- `redemptions[*].assets`
- `redemptions[*].expected_assets`
- `quotes[*].assets`
- `quotes[*].expected_assets`
- `account_views[*].locked_shares`
- `vault_views[*].pending_redemption_assets`
- `totals.excess_quote_assets`
- `risk.insolvent_vaults`
- `invariants.quotes_ok`

Si `assets` supera `expected_assets`, la secuencia ya demostro el bug de quote.
Si despues de `settle` aparece deficit, la secuencia tambien demostro impacto
contable sobre reservas.
