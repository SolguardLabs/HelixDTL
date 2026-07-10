# HelixDTL Protocol Notes

HelixDTL modela un vault DTL con una maquina de estados deliberadamente
pequena. La meta del lab no es simular un protocolo financiero completo, sino
aislar una clase de bug contable: la desincronizacion entre el indice con el que
un lock fue emitido y el indice con el que se cotiza su redencion.

## Entidades

### Account

Un account tiene assets externos y puede recibir assets tras retirar una
redencion liquidada. El modelo no incluye firmas ni permisos criptograficos; el
id numerico del account es la autoridad de la transicion.

Campos relevantes:

- `external_assets`: assets fuera del vault.
- `withdrawn_assets`: suma historica retirada desde redenciones.

### Vault

Un vault mantiene un `share_index` por epoch. Las shares liquidas capturan los
cambios de indice. Las locked shares quedan fuera del live supply mientras
esperan redencion.

Campos relevantes:

- `epoch`: epoch contable actual.
- `share_index`: assets por share liquida, escalado por `1_000_000`.
- `reserves`: assets disponibles para cubrir claims.
- `live_shares`: shares que siguen capturando revalorizaciones.
- `locked_shares`: shares bloqueadas y no yield bearing.
- `pending_assets`: assets ya cotizados pero no liquidados.
- `deficit_assets`: deficit materializado durante settlement.

### Position

Una position agrupa shares liquidas de un account en un vault. Cuando un usuario
deposita, recibe shares segun el indice actual. Cuando bloquea, se descuentan
shares liquidas de la position y se crea un lock.

### Lock

Un lock representa shares congeladas. El campo `issued_index` fija el indice
legitimo para su redencion. El campo `redemption_index` es el indice que el
motor usa al cotizar una solicitud de redencion.

En el modelo esperado, ambos campos deben permanecer equivalentes para locked
shares no yield bearing. En el modelo vulnerable, una transferencia interna tras
cambio de epoch puede crear un lock hijo con un `redemption_index` mas alto.

### Redemption

Una redemption se crea al consumir shares de un lock. No descuenta reservas al
instante: queda pendiente hasta `settle`. Esto permite estudiar ataques donde la
cotizacion incorrecta se acumula antes de materializarse como deficit.

## Transiciones

### Deposit

`deposit <account> <vault> <assets>`

El account entrega assets al vault y recibe `assets / share_index` shares. El
vault aumenta `reserves` y `live_shares`.

### Lock

`lock <account> <vault> <shares>`

Las shares salen del live supply y entran en locked supply. El lock captura el
indice actual como `issued_index` y `redemption_index`.

### Revalue

`revalue <vault> <new_index>`

El epoch avanza y las reservas se ajustan solo por las `live_shares`. Las locked
shares no capturan el incremento porque ya salieron del live supply.

### Transfer Lock

`transfer-lock <from> <to> <lock> <shares>`

Crea un lock hijo para el receptor y reduce el lock fuente. La regla correcta es
heredar `issued_index` y `redemption_index` del lock fuente. El lab contiene una
rama vulnerable que refresca `redemption_index` al indice actual si el vault
cambio de epoch desde que se abrio el lock fuente.

### Redeem

`redeem <account> <lock> <shares>`

Consume shares del lock y crea una redemption pendiente. La cotizacion vulnerable
usa `redemption_index`. El snapshot tambien calcula `expected_assets` con
`issued_index` para que el desvio sea visible.

### Quote Deposit

`quote-deposit <account> <vault> <assets>`

Calcula cuantas shares recibiria un deposito al indice actual del vault. No
debita al account ni acredita reservas. El resultado queda registrado en
`quotes` y marca `accepted = false` si la operacion no podria ejecutarse con el
estado actual.

### Quote Redeem

`quote-redeem <account> <lock> <shares>`

Calcula una redencion sin consumir shares del lock. El quote incluye
`expected_assets`, basado en `issued_index`, para que herramientas de auditoria
puedan comparar el valor esperado contra el valor de ejecucion.

### Settle

`settle <vault>`

Liquida todas las redenciones pendientes del vault para el epoch actual. Si el
total cotizado excede las reservas, el vault marca `insolvent` y acumula
`deficit_assets`.

### Withdraw

`withdraw <account> <redemption>`

Mueve los assets de una redencion ya liquidada al account. En este modelo el
deficit ya se materializo durante `settle`; withdraw solo refleja el pago final
en el balance externo del usuario.
