package com.rawlab.android

import android.graphics.Bitmap
import androidx.compose.foundation.Image
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Close
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties

/**
 * Full-screen LUT composer: pick two display-encoding look LUTs, tune both
 * strengths against the live photo preview, then bake the stack into a new
 * library LUT. Preview drives the render queue with composer-local settings;
 * baking runs the verified lattice composition natively.
 */
@Composable
fun LutComposerScreen(compose: LutComposeState, userLuts: List<UserLut>,
    onDismiss: () -> Unit, onRender: (String?, Float, String?, Float) -> Unit,
    onSave: (String?, Float, String?, Float, String) -> String?,
    onConsumeSaved: () -> Unit) {

    var lutA by remember { mutableStateOf<String?>(null) }
    var strengthA by rememberSaveable { mutableFloatStateOf(1f) }
    var lutB by remember { mutableStateOf<String?>(null) }
    var strengthB by rememberSaveable { mutableFloatStateOf(0.8f) }
    var pickSlot by rememberSaveable { mutableIntStateOf(0) }  // 0 none, 1 A, 2 B
    var name by rememberSaveable { mutableStateOf("") }
    var saved by remember { mutableStateOf<String?>(null) }

    @Composable
    fun nameOf(id: String?): String = userLuts.firstOrNull { it.id == id }?.name
        ?: stringResource(R.string.neutral)

    LaunchedEffect(Unit) { onRender(lutA, strengthA, lutB, strengthB) }

    // Re-render on release of a slider or a selection change.
    fun rerender() = onRender(lutA, strengthA, lutB, strengthB)

    Surface(Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
        Column(Modifier.fillMaxSize().padding(12.dp)) {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                Text(stringResource(R.string.composer_title), Modifier.weight(1f),
                    style = MaterialTheme.typography.titleMedium)
                IconButton(onClick = onDismiss) { Icon(Icons.Outlined.Close, stringResource(R.string.back)) }
            }

            BoxWithConstraints(Modifier.weight(1f).fillMaxWidth(), contentAlignment = Alignment.Center) {
                val preview = compose.preview
                if (compose.busy && preview == null) LinearProgressIndicator(Modifier.fillMaxWidth())
                preview?.let {
                    Image(it.asImageBitmap(), null, Modifier.fillMaxSize().padding(vertical = 4.dp),
                        contentScale = ContentScale.Fit)
                }
                if (compose.busy) LinearProgressIndicator(Modifier.fillMaxWidth().align(Alignment.BottomCenter))
            }

            Column(Modifier.fillMaxWidth().verticalScroll(rememberScrollState())) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(stringResource(R.string.composer_slot_a), Modifier.weight(1f), style = MaterialTheme.typography.labelLarge)
                    TextButton(onClick = { pickSlot = 1 }) { Text(nameOf(lutA), maxLines = 1) }
                }
                Slider(value = strengthA, onValueChange = { strengthA = it },
                    onValueChangeFinished = { rerender() }, valueRange = 0f..1f)
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(stringResource(R.string.composer_slot_b), Modifier.weight(1f), style = MaterialTheme.typography.labelLarge)
                    TextButton(onClick = { pickSlot = 2 }) { Text(nameOf(lutB), maxLines = 1) }
                }
                Slider(value = strengthB, onValueChange = { strengthB = it },
                    onValueChangeFinished = { rerender() }, valueRange = 0f..1f)

                Row(Modifier.fillMaxWidth().padding(top = 8.dp), verticalAlignment = Alignment.CenterVertically) {
                    OutlinedTextField(value = name, onValueChange = { name = it.take(40) },
                        label = { Text(stringResource(R.string.composer_name)) }, singleLine = true,
                        modifier = Modifier.weight(1f))
                    Spacer(Modifier.width(8.dp))
                    Button(onClick = {
                        val result = onSave(lutA, strengthA, lutB, strengthB, name)
                        saved = result
                    }, enabled = lutA != null && !compose.busy && name.isNotBlank()) {
                        Text(stringResource(R.string.composer_save))
                    }
                }
                saved?.let {
                    Text(stringResource(R.string.composer_saved, it), Modifier.padding(top = 6.dp),
                        color = MaterialTheme.colorScheme.primary, style = MaterialTheme.typography.bodySmall)
                    LaunchedEffect(it) {
                        kotlinx.coroutines.delay(1200)
                        onConsumeSaved()
                        onDismiss()
                    }
                }
            }
        }
    }

    if (pickSlot != 0) {
        Dialog(onDismissRequest = { pickSlot = 0 }, properties = DialogProperties(usePlatformDefaultWidth = true)) {
            Surface(shape = MaterialTheme.shapes.medium) {
                LazyColumn(Modifier.padding(vertical = 8.dp)) {
                    items(userLuts, key = { it.id }) { lut ->
                        ListItem(headlineContent = { Text(lut.name) },
                            modifier = Modifier.clickable {
                                if (pickSlot == 1) lutA = lut.id else lutB = lut.id
                                pickSlot = 0
                                rerender()
                            })
                    }
                }
            }
        }
    }
}
