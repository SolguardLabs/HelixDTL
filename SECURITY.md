# Security Policy

HelixDTL es un laboratorio CTF. La vulnerabilidad de separacion entre
`issued_index` y `redemption_index` es intencional.

## Fuera de alcance

- Reportes sobre falta de autenticacion real.
- Reportes sobre ausencia de criptografia.
- Reportes sobre precision financiera de produccion.
- Reportes que dependan de modificar el binario o el snapshot JSON.

## En alcance

- Secuencias validas que coticen locked shares por encima del indice emitido.
- Secuencias validas que generen deficit de reservas.
- Invariantes rotos durante settlement por epoch.
- Tests de regresion que demuestren una mitigacion.

## Mitigacion esperada

Una correccion razonable debe preservar el indice del lock fuente al crear un
lock hijo, incluso si el vault cambio de epoch. Tambien conviene rechazar
redenciones donde `quote_index > issued_index` para locked shares no yield
bearing y anadir un test que combine lock, transferencia interna, cambio de
epoch y redencion parcial.
